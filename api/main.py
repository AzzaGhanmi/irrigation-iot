import asyncio
from contextlib import asynccontextmanager
from pathlib import Path

import joblib
import pandas as pd
import requests
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

# ---------- 1) Paramètres ----------
FIREBASE_URL = "https://irrigation-iot-3b3ed-default-rtdb.europe-west1.firebasedatabase.app"
INTERVALLE_SECONDES = 5      # le pilote automatique travaille toutes les 5 secondes

DOSSIER_PROJET = Path(__file__).resolve().parent.parent
modele = joblib.load(DOSSIER_PROJET / "ia" / "modele_irrigation.joblib")
COLONNES = ["temperature", "humidite_air", "humidite_sol", "ph", "luminosite"]


# ---------- 2) Parler à Firebase (comme l'ESP32) ----------
def lire_firebase(chemin: str):
    reponse = requests.get(f"{FIREBASE_URL}{chemin}.json", timeout=10)
    reponse.raise_for_status()
    return reponse.json()


def ecrire_firebase(chemin: str, valeur):
    reponse = requests.put(f"{FIREBASE_URL}{chemin}.json", json=valeur, timeout=10)
    reponse.raise_for_status()
    return reponse.json()


# ---------- 3) Demander l'avis à l'IA ----------
def predire_irrigation(mesures: dict) -> dict:
    X = pd.DataFrame([{col: mesures[col] for col in COLONNES}])
    decision = int(modele.predict(X)[0])
    proba = float(modele.predict_proba(X)[0, 1])
    return {
        "decision": "ARROSER" if decision == 1 else "NE PAS ARROSER",
        "irrigation": decision,
        "probabilite_arroser": round(proba * 100, 1),
    }


# ---------- 4) LE PILOTE AUTOMATIQUE ----------
def cycle_automatique():
    """UN tour de boucle : lire -> décider -> commander la vanne."""
    mode = lire_firebase("/vanne/mode") or "auto"       # pas de mode -> "auto" par défaut
    if mode != "auto":
        print("[AUTO] Mode manuel : le pilote automatique ne touche à rien")
        return

    mesures = lire_firebase("/capteurs")
    if mesures is None:
        print("[AUTO] Aucune mesure dans Firebase pour l'instant")
        return

    resultat = predire_irrigation(mesures)
    commande = "OUVERTE" if resultat["irrigation"] == 1 else "FERMEE"

    ecrire_firebase("/vanne/commande", commande)                                  # ordre pour l'ESP32
    ecrire_firebase("/ia", {**resultat, "timestamp": {".sv": "timestamp"}})       # pour le dashboard

    print(f"[AUTO] Sol {mesures['humidite_sol']} % | Temp {mesures['temperature']} C "
          f"-> IA : {resultat['decision']} ({resultat['probabilite_arroser']} %) "
          f"-> vanne {commande}")


async def boucle_automatique():
    """Répète cycle_automatique() toutes les 5 secondes, pour toujours."""
    while True:
        try:
            await asyncio.to_thread(cycle_automatique)
        except Exception as erreur:
            print("[AUTO] Erreur :", erreur)     # une erreur ne doit jamais arrêter la boucle
        await asyncio.sleep(INTERVALLE_SECONDES)


@asynccontextmanager
async def lifespan(app: FastAPI):
    tache = asyncio.create_task(boucle_automatique())    # AU DÉMARRAGE : lancer le pilote
    print("[AUTO] Pilote automatique démarré")
    yield                                                 # ... le serveur tourne ...
    tache.cancel()                                        # À L'ARRÊT : stopper le pilote


app = FastAPI(title="API Irrigation intelligente", lifespan=lifespan)


# ---------- 5) Le "formulaire" des mesures ----------
class Mesures(BaseModel):
    temperature:  float = Field(ge=-40, le=80,  examples=[38])
    humidite_air: float = Field(ge=0,   le=100, examples=[30])
    humidite_sol: float = Field(ge=0,   le=100, examples=[22])
    ph:           float = Field(ge=0,   le=14,  examples=[6.5])
    luminosite:   float = Field(ge=0,   le=100, examples=[90])


# ---------- 6) Les routes de l'API ----------
@app.get("/")
def accueil():
    return {"message": "Bienvenue sur l'API d'irrigation intelligente"}


@app.post("/predire")
def predire(mesures: Mesures):
    """Prédiction à partir de mesures envoyées à la main."""
    return predire_irrigation(mesures.model_dump())


@app.get("/capteurs")
def capteurs():
    """Dernières mesures envoyées par l'ESP32 (lues dans Firebase)."""
    donnees = lire_firebase("/capteurs")
    if donnees is None:
        raise HTTPException(status_code=404, detail="Aucune mesure dans Firebase")
    return donnees


@app.get("/decision")
def decision():
    """Lit les vraies mesures dans Firebase, puis demande l'avis à l'IA."""
    mesures = lire_firebase("/capteurs")
    if mesures is None:
        raise HTTPException(status_code=404, detail="Aucune mesure dans Firebase")
    return {"mesures": mesures, **predire_irrigation(mesures)}