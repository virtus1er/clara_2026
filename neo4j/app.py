import spacy
from spacy.lang.fr.stop_words import STOP_WORDS as fr_stop
from typing import List, Tuple, Dict, Set, Optional, Any
from functools import lru_cache
from dataclasses import dataclass, field
import numpy as np


@dataclass
class WordWithEmotions:
    """Un mot avec ses sentence_ids et émotions associées"""
    word: str
    emotional_states: Dict[int, List[float]] = field(default_factory=dict)
    
    def add_state(self, sentence_id: int, emotions: List[float] = None):
        """Ajoute un état émotionnel pour un sentence_id"""
        if emotions is None:
            emotions = [0.0] * 24
        self.emotional_states[sentence_id] = emotions
    
    @property
    def sentence_ids(self) -> List[int]:
        return sorted(list(self.emotional_states.keys()))
    
    def get_avg_emotions(self) -> List[float]:
        """Retourne les émotions moyennes"""
        if not self.emotional_states:
            return [0.0] * 24
        emotions_array = list(self.emotional_states.values())
        return [sum(e[i] for e in emotions_array) / len(emotions_array) 
                for i in range(24)]
    
    def get_emotional_variance(self) -> float:
        """Retourne la variance émotionnelle (stabilité)"""
        if len(self.emotional_states) < 2:
            return 0.0
        emotions_array = np.array(list(self.emotional_states.values()))
        return float(np.mean(np.var(emotions_array, axis=0)))
    
    def to_dict(self) -> Dict:
        return {
            'word': self.word,
            'sentence_ids': self.sentence_ids,
            'emotional_states': {str(k): v for k, v in self.emotional_states.items()},
            'avg_emotions': self.get_avg_emotions(),
            'emotional_variance': self.get_emotional_variance()
        }


@dataclass
class RelationWithEmotions:
    """Une relation avec les états émotionnels par sentence_id"""
    source: str
    relation: str
    target: str
    emotional_states: Dict[int, List[float]] = field(default_factory=dict)
    
    def add_state(self, sentence_id: int, emotions: List[float] = None):
        if emotions is None:
            emotions = [0.0] * 24
        self.emotional_states[sentence_id] = emotions
    
    @property
    def sentence_ids(self) -> List[int]:
        return sorted(list(self.emotional_states.keys()))
    
    def get_avg_emotions(self) -> List[float]:
        if not self.emotional_states:
            return [0.0] * 24
        emotions_array = list(self.emotional_states.values())
        return [sum(e[i] for e in emotions_array) / len(emotions_array) 
                for i in range(24)]
    
    def to_tuple(self) -> Tuple[str, str, str, Dict[int, List[float]]]:
        return (self.source, self.relation, self.target, self.emotional_states)
    
    def to_dict(self) -> Dict:
        return {
            'source': self.source,
            'relation': self.relation,
            'target': self.target,
            'sentence_ids': self.sentence_ids,
            'emotional_states': {str(k): v for k, v in self.emotional_states.items()},
            'avg_emotions': self.get_avg_emotions()
        }


class EmotionalAnalyzer:
    """Analyseur d'historique émotionnel"""
    
    EMOTION_NAMES = [
        'joie', 'confiance', 'peur', 'surprise', 'tristesse', 
        'dégoût', 'colère', 'anticipation', 'sérénité', 'intérêt',
        'acceptation', 'appréhension', 'distraction', 'ennui', 'contrariété',
        'pensivité', 'extase', 'admiration', 'terreur', 'étonnement',
        'chagrin', 'aversion', 'rage', 'vigilance'
    ]
    
    @staticmethod
    def get_dominant(emotions: List[float]) -> str:
        """Retourne l'émotion dominante"""
        if not emotions or all(e == 0 for e in emotions):
            return 'Neutre'
        max_idx = emotions.index(max(emotions))
        if max_idx < len(EmotionalAnalyzer.EMOTION_NAMES):
            return EmotionalAnalyzer.EMOTION_NAMES[max_idx].capitalize()
        return 'Inconnu'
    
    @staticmethod
    def compute_valence(emotions: List[float]) -> float:
        """Calcule la valence (-1 à 1) à partir des émotions"""
        if not emotions:
            return 0.0
        # Émotions positives: joie, confiance, sérénité, intérêt, acceptation, extase, admiration
        positive_indices = [0, 1, 8, 9, 10, 16, 17]
        # Émotions négatives: peur, tristesse, dégoût, colère, appréhension, ennui, chagrin, aversion, rage
        negative_indices = [2, 4, 5, 6, 11, 13, 20, 21, 22]
        
        positive = sum(emotions[i] for i in positive_indices if i < len(emotions))
        negative = sum(emotions[i] for i in negative_indices if i < len(emotions))
        
        total = positive + negative
        if total == 0:
            return 0.0
        return (positive - negative) / total
    
    @staticmethod
    def compute_intensity(emotions: List[float]) -> float:
        """Calcule l'intensité émotionnelle (0 à 1)"""
        if not emotions:
            return 0.0
        return min(1.0, sum(emotions) / len(emotions) * 2)
    
    @staticmethod
    def analyze_history(emotional_states: Dict[int, List[float]]) -> Dict:
        """Analyse complète d'un historique émotionnel"""
        if not emotional_states:
            return {
                'avg_emotions': [0.0] * 24,
                'variance': 0.0,
                'stability': 1.0,
                'trajectory': 'stable',
                'trauma_score': 0.0,
                'dominant_emotion': 'Neutre',
                'avg_valence': 0.0,
                'avg_intensity': 0.0,
                'emotion_count': 0
            }
        
        emotions_array = np.array(list(emotional_states.values()))
        avg_emotions = np.mean(emotions_array, axis=0).tolist()
        variance = float(np.mean(np.var(emotions_array, axis=0)))
        
        # Trajectoire émotionnelle
        if len(emotional_states) >= 3:
            valences = [EmotionalAnalyzer.compute_valence(e) for e in emotional_states.values()]
            trend = np.polyfit(range(len(valences)), valences, 1)[0]
            if trend > 0.1:
                trajectory = 'ascending'
            elif trend < -0.1:
                trajectory = 'descending'
            elif variance > 0.3:
                trajectory = 'volatile'
            else:
                trajectory = 'stable'
        else:
            trajectory = 'stable'
        
        # Score de trauma (émotions négatives intenses et récurrentes)
        negative_indices = [2, 4, 5, 6, 11, 20, 21, 22]  # peur, tristesse, dégoût, colère...
        trauma_emotions = [[e[i] for i in negative_indices if i < len(e)] for e in emotional_states.values()]
        trauma_score = float(np.mean([max(te) if te else 0 for te in trauma_emotions]))
        
        return {
            'avg_emotions': avg_emotions,
            'variance': variance,
            'stability': max(0.0, 1.0 - variance * 2),
            'trajectory': trajectory,
            'trauma_score': trauma_score,
            'dominant_emotion': EmotionalAnalyzer.get_dominant(avg_emotions),
            'avg_valence': EmotionalAnalyzer.compute_valence(avg_emotions),
            'avg_intensity': EmotionalAnalyzer.compute_intensity(avg_emotions),
            'emotion_count': len(emotional_states)
        }


class RelationExtractor:
    """Extracteur de relations sémantiques à partir de texte français"""

    def __init__(self, model_name: str = "fr_core_news_lg"):
        self.nlp = spacy.load(model_name)
        self.analyzer = EmotionalAnalyzer()

        # Mapping des dépendances syntaxiques -> types de relations
        self.DEPENDENCY_PATTERNS = {
            ('det', 'DET'): 0,
            ('det', 'ADP'): 0,
            ('det', 'ADV'): 0,
            ('nsubj', 'PROPN'): 0,
            ('nsubj', 'PRON'): 0,
            ('nsubj', 'NOUN'): 0,
            ('case', 'ADP'): 0,
            ('amod', 'ADJ'): 'EST',
            ('amod', 'VERB'): 'EST',
            ('nmod', 'NOUN'): 0,
            ('nmod', 'ADJ'): 0,
            ('obj', 'NOUN'): 0,
            ('obj', 'PROPN'): 0,
            ('obj', 'PRON'): 0,
            ('obj', 'ADJ'): 0,
            ('conj', 'ADJ'): 0,
            ('conj', 'VERB'): 0,
            ('conj', 'NOUN'): 0,
            ('conj', 'PROPN'): 0,
            ('acl:relcl', 'ADJ'): 'EST',
            ('acl:relcl', 'VERB'): 'CARACTERISE',
            ('acl', 'ADJ'): 0,
            ('acl', 'VERB'): 0,
            ('cc', 'CCONJ'): 0,
            ('advmod', 'ADV'): 0,
            ('advmod', 'ADJ'): 0,
            ('obl:arg', 'NOUN'): 0,
            ('obl:arg', 'ADJ'): 0,
            ('obl:mod', 'NOUN'): 'LOCALISE',
            ('cop', 'AUX'): 0,
            ('expl:subj', 'PRON'): 0,
            ('expl:comp', 'PRON'): 0,
            ('dep', 'VERB'): 0,
            ('dep', 'ADV'): 0,
            ('dep', 'PRON'): 0,
            ('dep', 'NOUN'): 0,
            ('aux:tense', 'AUX'): 0,
            ('aux:tense', 'PROPN'): 0,
            ('aux:pass', 'AUX'): 0,
            ('flat:name', 'ADJ'): 0,
            ('flat:name', 'NOUN'): 0,
            ('xcomp', 'NOUN'): 0,
            ('xcomp', 'VERB'): 'IMPLIQUE',
            ('xcomp', 'ADJ'): 0,
            ('nummod', 'NUM'): 'QUANTIFIE',
            ('mark', 'ADP'): 0,
            ('mark', 'SCONJ'): 0,
            ('mark', 'ADV'): 0,
            ('fixed', 'SCONJ'): 0,
            ('advcl', 'VERB'): 'ASSOCIE',
            ('advcl', 'NOUN'): 0,
            ('iobj', 'PRON'): 0,
            ('ccomp', 'VERB'): 0,
            ('obl:agent', 'NOUN'): 0,
            ('obl:agent', 'PROPN'): 0,
        }

        self._pos_cache = {}
        self._lemma_cache = {}
        self._sentence_counter = 0

    def reset_sentence_counter(self, start_value: int = 0):
        """Réinitialise le compteur de phrases"""
        self._sentence_counter = start_value

    def get_next_sentence_id(self) -> int:
        """Retourne et incrémente le prochain ID de phrase"""
        self._sentence_counter += 1
        return self._sentence_counter

    def extract(self, text: str, sentence_id: Optional[int] = None, 
                emotions: Optional[List[float]] = None) -> Tuple[List[Dict], List[Dict]]:
        """
        Extrait les mots significatifs et les relations d'un texte avec émotions.

        Args:
            text: Le texte à analyser
            sentence_id: ID de phrase optionnel (auto-généré si non fourni)
            emotions: Vecteur de 24 émotions associé à cette phrase

        Returns:
            Tuple[List[Dict], List[Dict]]:
                (mots_avec_emotions, relations_avec_emotions)
        """
        if sentence_id is None:
            sentence_id = self.get_next_sentence_id()
        
        if emotions is None:
            emotions = [0.0] * 24
            
        doc = self.nlp(text)
        mots_significatifs = self._extract_significant_words(text)
        triplets = self._extract_triplets(doc)
        relations = self._extract_all_relations(mots_significatifs, triplets, doc)

        # Convertir en format avec émotions
        mots_avec_emotions = [
            {
                'word': mot,
                'sentence_ids': [sentence_id],
                'emotional_states': {sentence_id: emotions}
            }
            for mot in mots_significatifs
        ]
        
        relations_avec_emotions = [
            {
                'source': rel[0],
                'relation': rel[1],
                'target': rel[2],
                'sentence_ids': [sentence_id],
                'emotional_states': {sentence_id: emotions}
            }
            for rel in relations
        ]

        return mots_avec_emotions, relations_avec_emotions

    def extract_batch(self, entries: List[Dict], 
                      start_sentence_id: Optional[int] = None) -> Tuple[Dict[str, WordWithEmotions], Dict[str, RelationWithEmotions]]:
        """
        Extrait les mots et relations de plusieurs phrases avec fusion des états émotionnels.
        
        Args:
            entries: Liste de {'text': str, 'emotions': [24 floats], 'sentence_id': int (optionnel)}
            start_sentence_id: ID de départ (utilise le compteur interne si non fourni)
            
        Returns:
            Tuple[Dict[str, WordWithEmotions], Dict[str, RelationWithEmotions]]:
                (mots_fusionnés, relations_fusionnées)
        """
        if start_sentence_id is not None:
            self._sentence_counter = start_sentence_id - 1
        
        words_map: Dict[str, WordWithEmotions] = {}
        relations_map: Dict[str, RelationWithEmotions] = {}
        
        for entry in entries:
            text = entry.get('text', '')
            emotions = entry.get('emotions', [0.0] * 24)
            sentence_id = entry.get('sentence_id', self.get_next_sentence_id())
            
            mots, relations = self.extract(text, sentence_id, emotions)
            
            # Fusionner les mots
            for mot_dict in mots:
                word = mot_dict['word'].lower()
                if word not in words_map:
                    words_map[word] = WordWithEmotions(word=word)
                words_map[word].add_state(sentence_id, emotions)
            
            # Fusionner les relations
            for rel_dict in relations:
                key = f"{rel_dict['source'].lower()}|{rel_dict['relation']}|{rel_dict['target'].lower()}"
                if key not in relations_map:
                    relations_map[key] = RelationWithEmotions(
                        source=rel_dict['source'].lower(),
                        relation=rel_dict['relation'],
                        target=rel_dict['target'].lower()
                    )
                relations_map[key].add_state(sentence_id, emotions)
        
        return words_map, relations_map

    def extract_legacy(self, text: str) -> Tuple[List[str], List[Tuple[str, str, str]]]:
        """
        Version legacy sans émotions (pour rétrocompatibilité).
        """
        doc = self.nlp(text)
        mots_significatifs = self._extract_significant_words(text)
        triplets = self._extract_triplets(doc)
        relations = self._extract_all_relations(mots_significatifs, triplets, doc)
        return mots_significatifs, relations

    def _extract_significant_words(self, text: str) -> List[str]:
        """Extrait les mots significatifs du texte"""
        doc = self.nlp(text.lower())
        mots = []

        for token in doc:
            if (token.text not in fr_stop and
                    token.pos_ not in ["PUNCT", "DET", "ADP", "AUX", "CCONJ", "SCONJ", "PRON"] and
                    not token.is_punct and
                    len(token.text) > 1):
                mots.append(token.text)

        return mots

    def _extract_triplets(self, doc) -> List[Tuple]:
        """Extrait les triplets de dépendances syntaxiques"""
        triplets = []

        for token in doc:
            for child in token.children:
                if child.dep_ == "punct":
                    continue

                pattern = (child.dep_, child.pos_)
                relation = self.DEPENDENCY_PATTERNS.get(pattern, 0)
                triplets.append((child.lemma_, relation, child.head.lemma_))

        return triplets

    @lru_cache(maxsize=1000)
    def _normalize(self, word: str) -> str:
        return word.lower().strip()

    def _get_pos(self, word: str) -> Optional[str]:
        if word not in self._pos_cache:
            doc = self.nlp(word)
            self._pos_cache[word] = doc[0].pos_ if doc else None
        return self._pos_cache[word]

    def _get_lemma(self, word: str) -> str:
        if word not in self._lemma_cache:
            doc = self.nlp(word)
            self._lemma_cache[word] = doc[0].lemma_ if doc else word
        return self._lemma_cache[word]

    def _find_in_significatifs(self, word: str, significatifs: List[str]) -> Optional[str]:
        """Trouve un mot dans la liste des mots significatifs"""
        word_norm = self._normalize(word)

        for sig in significatifs:
            if self._normalize(sig) == word_norm:
                return sig

        word_lemma = self._get_lemma(word_norm)
        for sig in significatifs:
            sig_norm = self._normalize(sig)
            sig_lemma = self._get_lemma(sig_norm)
            if word_lemma == sig_lemma or word_norm == sig_lemma or word_lemma == sig_norm:
                return sig

        for sig in significatifs:
            sig_norm = self._normalize(sig)
            if word_norm + 's' == sig_norm or sig_norm + 's' == word_norm:
                return sig
            if word_norm + 'x' == sig_norm or sig_norm + 'x' == word_norm:
                return sig

        for sig in significatifs:
            sig_norm = self._normalize(sig)
            if len(word_norm) >= 4 and len(sig_norm) >= 4:
                if word_norm[:4] == sig_norm[:4]:
                    return sig

        return None

    def _analyze_semantic_relation(self, word1: str, word2: str) -> str:
        """Détermine le type de relation sémantique entre deux mots"""
        pos1 = self._get_pos(word1)
        pos2 = self._get_pos(word2)

        if not pos1 or not pos2:
            return 'ASSOCIE'

        if pos1 == 'PROPN' and pos2 == 'VERB':
            return 'FAIT'
        elif pos2 == 'PROPN' and pos1 == 'VERB':
            return 'FAIT'
        elif pos1 == 'ADJ' and pos2 == 'NOUN':
            return 'EST'
        elif pos2 == 'ADJ' and pos1 == 'NOUN':
            return 'EST'
        elif pos1 == 'ADV' and pos2 == 'VERB':
            return 'MODIFIE'
        elif pos2 == 'ADV' and pos1 == 'VERB':
            return 'MODIFIE'
        elif pos1 == 'NOUN' and pos2 == 'VERB':
            return 'UTILISE'
        elif pos2 == 'NOUN' and pos1 == 'VERB':
            return 'UTILISE'
        elif pos1 == 'PROPN' and pos2 == 'NOUN':
            return 'POSSEDE'
        elif pos2 == 'PROPN' and pos1 == 'NOUN':
            return 'POSSEDE'
        elif pos1 == 'NOUN' and pos2 == 'NOUN':
            return 'RELIE'

        return 'ASSOCIE'

    def _extract_all_relations(self, significatifs: List[str], triplets: List[Tuple],
                               doc) -> List[Tuple[str, str, str]]:
        """Extrait toutes les relations"""
        all_relations = []

        all_relations.extend(self._extract_direct_relations(significatifs, triplets, doc))
        all_relations.extend(self._extract_possession_relations(significatifs, doc))
        all_relations.extend(self._extract_coordinated_attributes(significatifs, triplets))
        all_relations.extend(self._extract_verb_complement_relations(significatifs, doc))
        all_relations.extend(self._extract_quantification_relations(significatifs, doc))
        all_relations.extend(self._extract_relative_relations(significatifs, doc))
        all_relations.extend(self._extract_zero_relations(significatifs, triplets))
        all_relations.extend(self._extract_location_relations(significatifs, doc))

        seen = set()
        unique = []
        for rel in all_relations:
            if len(rel) == 3 and all(r and str(r).strip() for r in rel):
                w1 = self._find_in_significatifs(rel[0], significatifs) or rel[0]
                w2 = self._find_in_significatifs(rel[2], significatifs) or rel[2]

                if w1 in significatifs and w2 in significatifs:
                    key = (w1, rel[1], w2)
                    rev = (w2, rel[1], w1)
                    if key not in seen and rev not in seen:
                        seen.add(key)
                        unique.append((w1, rel[1], w2))

        used = {r[0] for r in unique} | {r[2] for r in unique}
        unique.extend(self._cover_unused_words(significatifs, used, triplets, doc))

        return unique

    def _extract_direct_relations(self, significatifs: List[str], triplets: List[Tuple],
                                  doc) -> List[Tuple[str, str, str]]:
        relations = []

        for word1, relation, word2 in triplets:
            if str(relation) != '0':
                sig1 = self._find_in_significatifs(word1, significatifs)
                sig2 = self._find_in_significatifs(word2, significatifs)

                if sig1 and sig2:
                    rel_type = str(relation).upper()
                    if rel_type == 'IMPLIQUE':
                        relations.append((sig2, rel_type, sig1))
                    else:
                        relations.append((sig1, rel_type, sig2))
                elif sig2 and not sig1 and str(relation).upper() == 'IMPLIQUE':
                    for token in doc:
                        if token.lemma_.lower() == word1.lower():
                            for child in token.children:
                                if child.dep_ in ('obj', 'obl:arg', 'obl:mod', 'iobj'):
                                    sig_obj = self._find_in_significatifs(child.text, significatifs)
                                    if sig_obj and sig_obj != sig2:
                                        relations.append((sig2, 'CONCERNE', sig_obj))
                                        break

        return relations

    def _extract_possession_relations(self, significatifs: List[str], doc) -> List[Tuple[str, str, str]]:
        relations = []

        for token in doc:
            if token.pos_ == 'VERB':
                sujet = objet = None
                for child in token.children:
                    if child.dep_ == 'nsubj':
                        sujet = self._find_in_significatifs(child.text, significatifs)
                    elif child.dep_ in ('obj', 'obl:arg'):
                        objet = self._find_in_significatifs(child.text, significatifs)

                if sujet and objet and sujet != objet:
                    if self._get_pos(sujet) == 'PROPN' and self._get_pos(objet) == 'NOUN':
                        relations.append((sujet, 'POSSEDE', objet))

        return relations

    def _extract_coordinated_attributes(self, significatifs: List[str],
                                        triplets: List[Tuple]) -> List[Tuple[str, str, str]]:
        relations = []
        est_relations = {}

        for word1, relation, word2 in triplets:
            if str(relation).upper() == 'EST':
                sig1 = self._find_in_significatifs(word1, significatifs)
                sig2 = self._find_in_significatifs(word2, significatifs)
                if sig1 and sig2:
                    est_relations.setdefault(sig2, []).append(sig1)

        for objet, attributs in est_relations.items():
            for word1, relation, word2 in triplets:
                if str(relation) == '0':
                    sig1 = self._find_in_significatifs(word1, significatifs)
                    sig2 = self._find_in_significatifs(word2, significatifs)

                    if sig1 in attributs and sig2 and sig2 not in attributs:
                        if self._get_pos(sig1) == self._get_pos(sig2):
                            relations.append((sig2, 'EST', objet))

        return relations

    def _extract_verb_complement_relations(self, significatifs: List[str],
                                           doc) -> List[Tuple[str, str, str]]:
        relations = []

        for token in doc:
            if token.pos_ == 'VERB':
                sig_verb = self._find_in_significatifs(token.text, significatifs)
                if not sig_verb:
                    sig_verb = self._find_in_significatifs(token.lemma_, significatifs)

                if sig_verb:
                    for child in token.children:
                        if child.dep_ in ('xcomp', 'ccomp') and child.pos_ == 'VERB':
                            sig_inf = self._find_in_significatifs(child.text, significatifs)
                            if not sig_inf:
                                sig_inf = self._find_in_significatifs(child.lemma_, significatifs)

                            if sig_inf and sig_verb != sig_inf:
                                relations.append((sig_verb, 'IMPLIQUE', sig_inf))
                            elif not sig_inf:
                                for gc in child.children:
                                    if gc.dep_ in ('obj', 'obl:arg', 'obl:mod'):
                                        sig_obj = self._find_in_significatifs(gc.text, significatifs)
                                        if sig_obj and sig_verb != sig_obj:
                                            relations.append((sig_verb, 'CONCERNE', sig_obj))
                                            break

        return relations

    def _extract_quantification_relations(self, significatifs: List[str],
                                          doc) -> List[Tuple[str, str, str]]:
        relations = []

        for token in doc:
            if token.pos_ == 'NUM' and token.head and token.head.pos_ == 'NOUN':
                sig_num = self._find_in_significatifs(token.text, significatifs)
                sig_noun = self._find_in_significatifs(token.head.text, significatifs)
                if not sig_noun:
                    sig_noun = self._find_in_significatifs(token.head.lemma_, significatifs)

                if sig_num and sig_noun and sig_num != sig_noun:
                    relations.append((sig_num, 'QUANTIFIE', sig_noun))
                elif sig_noun and not sig_num:
                    for other in doc:
                        if other.pos_ in ('NOUN', 'PROPN', 'VERB'):
                            sig_other = self._find_in_significatifs(other.text, significatifs)
                            if sig_other and sig_other != sig_noun:
                                relations.append((sig_other, 'RELIE', sig_noun))
                                break

        return relations

    def _extract_relative_relations(self, significatifs: List[str],
                                    doc) -> List[Tuple[str, str, str]]:
        relations = []

        for token in doc:
            if token.dep_ in ('acl:relcl', 'acl'):
                sig_verb = self._find_in_significatifs(token.text, significatifs)
                if not sig_verb:
                    sig_verb = self._find_in_significatifs(token.lemma_, significatifs)
                sig_head = self._find_in_significatifs(token.head.text, significatifs)

                if sig_verb and sig_head and sig_verb != sig_head:
                    relations.append((sig_verb, 'CARACTERISE', sig_head))

        return relations

    def _extract_zero_relations(self, significatifs: List[str],
                                triplets: List[Tuple]) -> List[Tuple[str, str, str]]:
        relations = []

        for word1, relation, word2 in triplets:
            if str(relation) == '0':
                sig1 = self._find_in_significatifs(word1, significatifs)
                sig2 = self._find_in_significatifs(word2, significatifs)

                if sig1 and sig2 and sig1 != sig2:
                    rel_type = self._analyze_semantic_relation(sig1, sig2)
                    relations.append((sig1, rel_type, sig2))

        return relations

    def _extract_location_relations(self, significatifs: List[str],
                                    doc) -> List[Tuple[str, str, str]]:
        relations = []
        location_preps = {'dans', 'sur', 'au', 'à', 'en', 'chez', 'vers', 'sous', 'devant', 'derrière'}
        
        for token in doc:
            if token.dep_ in ('obl:mod', 'obl:arg', 'nmod'):
                has_location_prep = False
                for child in token.children:
                    if child.dep_ == 'case' and child.text.lower() in location_preps:
                        has_location_prep = True
                        break
                
                if has_location_prep:
                    sig_location = self._find_in_significatifs(token.text, significatifs)
                    if not sig_location:
                        sig_location = self._find_in_significatifs(token.lemma_, significatifs)
                    
                    head = token.head
                    if head.pos_ == 'VERB':
                        for sibling in head.children:
                            if sibling.dep_ in ('obj', 'nsubj') and sibling != token:
                                sig_obj = self._find_in_significatifs(sibling.text, significatifs)
                                if not sig_obj:
                                    sig_obj = self._find_in_significatifs(sibling.lemma_, significatifs)
                                
                                if sig_obj and sig_location and sig_obj != sig_location:
                                    relations.append((sig_obj, 'LOCALISE', sig_location))
                    
        return relations

    def _cover_unused_words(self, significatifs: List[str], used: Set[str],
                            triplets: List[Tuple], doc) -> List[Tuple[str, str, str]]:
        relations = []
        unused = [w for w in significatifs if w not in used]

        for word in unused:
            for w1, rel, w2 in triplets:
                sig1 = self._find_in_significatifs(w1, significatifs)
                sig2 = self._find_in_significatifs(w2, significatifs)

                if sig1 == word and sig2 in used:
                    relations.append((sig1, self._analyze_semantic_relation(sig1, sig2), sig2))
                    break
                elif sig2 == word and sig1 in used:
                    relations.append((sig1, self._analyze_semantic_relation(sig1, sig2), sig2))
                    break
        return relations


if __name__ == "__main__":
    ex = RelationExtractor()

    # Tests avec émotions par sentence_id
    print("=" * 80)
    print("TEST AVEC ÉMOTIONS PAR SENTENCE_ID")
    print("=" * 80)
    
    # Simuler des entrées avec émotions
    entries = [
        {
            'text': "Dans le parc, j'ai vu des canards.",
            'emotions': [0.8, 0.2, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.5, 0.3] + [0.0] * 14,
            'sentence_id': 1
        },
        {
            'text': "Au jardin, il y a des fleurs.",
            'emotions': [0.7, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.2] + [0.0] * 14,
            'sentence_id': 2
        },
        {
            'text': "Je me promenais dans le parc quand il a commencé à pleuvoir.",
            'emotions': [0.1, 0.0, 0.3, 0.4, 0.2, 0.0, 0.0, 0.0, 0.0, 0.1] + [0.0] * 14,
            'sentence_id': 4
        },
        {
            'text': "Le parc est magnifique au printemps.",
            'emotions': [0.9, 0.4, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.7, 0.5] + [0.0] * 14,
            'sentence_id': 12
        }
    ]
    
    words_map, relations_map = ex.extract_batch(entries)
    
    print("\n" + "=" * 80)
    print("MOTS AVEC HISTORIQUE ÉMOTIONNEL")
    print("=" * 80)
    
    for word, w_obj in sorted(words_map.items()):
        print(f"\n'{word}':")
        print(f"  Sentence IDs: {w_obj.sentence_ids}")
        print(f"  États émotionnels:")
        for sid, emotions in w_obj.emotional_states.items():
            dominant = EmotionalAnalyzer.get_dominant(emotions)
            valence = EmotionalAnalyzer.compute_valence(emotions)
            print(f"    [{sid}]: dominant={dominant}, valence={valence:.2f}")
        
        analysis = EmotionalAnalyzer.analyze_history(w_obj.emotional_states)
        print(f"  Analyse globale:")
        print(f"    - Dominant: {analysis['dominant_emotion']}")
        print(f"    - Valence moyenne: {analysis['avg_valence']:.2f}")
        print(f"    - Stabilité: {analysis['stability']:.2f}")
        print(f"    - Trajectoire: {analysis['trajectory']}")
        print(f"    - Score trauma: {analysis['trauma_score']:.2f}")
    
    print("\n" + "=" * 80)
    print("FOCUS SUR 'PARC' (apparaît dans 3 phrases avec émotions différentes)")
    print("=" * 80)
    
    if 'parc' in words_map:
        parc = words_map['parc']
        print(f"\nHistorique émotionnel de 'parc':")
        print(f"  emotional_states: {{")
        for sid, emotions in parc.emotional_states.items():
            dominant = EmotionalAnalyzer.get_dominant(emotions)
            print(f"    {sid}: {emotions[:5]}... -> {dominant}")
        print(f"  }}")
        
        analysis = EmotionalAnalyzer.analyze_history(parc.emotional_states)
        print(f"\n  Trajectoire émotionnelle: {analysis['trajectory']}")
        print(f"  Le concept 'parc' est généralement {analysis['dominant_emotion'].lower()}")
        print(f"  avec une stabilité de {analysis['stability']:.0%}")
