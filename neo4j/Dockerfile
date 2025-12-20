FROM python:3.10-slim

# Dépendances système
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    curl \
    git \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# Définir le dossier de travail
WORKDIR /app

# Copier les dépendances
COPY requirements.txt .

# Installer les packages Python nécessaires
RUN pip install --no-cache-dir -r requirements.txt

# Mettre à jour spaCy pour compatibilité avec le modèle 3.7.0
RUN pip install --upgrade "spacy>=3.7.0,<3.8.0"

# Installer le modèle français spaCy
RUN pip install https://github.com/explosion/spacy-models/releases/download/fr_core_news_lg-3.7.0/fr_core_news_lg-3.7.0-py3-none-any.whl

# Installer coreferee et le modèle français
#RUN pip install coreferee && \
#    python -m coreferee install fr

# Copier le reste du code dans l'image
COPY . .

# Point d'entrée
CMD ["python", "app.py"]
