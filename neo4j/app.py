import spacy
from spacy.lang.fr.stop_words import STOP_WORDS as fr_stop
from typing import List, Tuple, Dict, Set, Optional
from functools import lru_cache


class RelationExtractor:
    """Extracteur de relations sémantiques à partir de texte français"""

    def __init__(self, model_name: str = "fr_core_news_lg"):
        self.nlp = spacy.load(model_name)

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
            ('obl:mod', 'NOUN'): 0,
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
            ('advcl', 'VERB'): 0,
            ('advcl', 'NOUN'): 0,
            ('iobj', 'PRON'): 0,
            ('ccomp', 'VERB'): 0,
            ('obl:agent', 'NOUN'): 0,
            ('obl:agent', 'PROPN'): 0,
        }

        self._pos_cache = {}
        self._lemma_cache = {}

    def extract(self, text: str) -> Tuple[List[str], List[Tuple[str, str, str]]]:
        """
        Extrait les mots significatifs et les relations d'un texte.

        Returns:
            Tuple[List[str], List[Tuple[str, str, str]]]:
                (mots_significatifs, relations)
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

        # Correspondance exacte
        for sig in significatifs:
            if self._normalize(sig) == word_norm:
                return sig

        # Correspondance par lemme
        word_lemma = self._get_lemma(word_norm)
        for sig in significatifs:
            sig_norm = self._normalize(sig)
            sig_lemma = self._get_lemma(sig_norm)
            if word_lemma == sig_lemma or word_norm == sig_lemma or word_lemma == sig_norm:
                return sig

        # Correspondance singulier/pluriel
        for sig in significatifs:
            sig_norm = self._normalize(sig)
            if word_norm + 's' == sig_norm or sig_norm + 's' == word_norm:
                return sig
            if word_norm + 'x' == sig_norm or sig_norm + 'x' == word_norm:
                return sig

        # Correspondance par préfixe
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

        # 1. Relations directes explicites
        all_relations.extend(self._extract_direct_relations(significatifs, triplets, doc))

        # 2. Relations de possession
        all_relations.extend(self._extract_possession_relations(significatifs, doc))

        # 3. Attributs coordonnés
        all_relations.extend(self._extract_coordinated_attributes(significatifs, triplets))

        # 4. Relations verbes + compléments
        all_relations.extend(self._extract_verb_complement_relations(significatifs, doc))

        # 5. Relations de quantification
        all_relations.extend(self._extract_quantification_relations(significatifs, doc))

        # 6. Relations relatives
        all_relations.extend(self._extract_relative_relations(significatifs, doc))

        # 7. Relations zéro
        all_relations.extend(self._extract_zero_relations(significatifs, triplets))

        # Dédoublonner
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

        # Couverture des mots non utilisés
        used = {r[0] for r in unique} | {r[2] for r in unique}
        unique.extend(self._cover_unused_words(significatifs, used, triplets, doc))

        return unique

    def _extract_direct_relations(self, significatifs: List[str], triplets: List[Tuple],
                                  doc) -> List[Tuple[str, str, str]]:
        """Extrait les relations directes explicites"""
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
                    # Chercher l'objet de l'infinitif
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
        """Extrait les relations de possession"""
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
        """Extrait les attributs coordonnés"""
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
        """Extrait les relations verbe + complément verbal"""
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
        """Extrait les relations de quantification"""
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
                    # Chercher un autre mot significatif
                    for other in doc:
                        if other.pos_ in ('NOUN', 'PROPN', 'VERB'):
                            sig_other = self._find_in_significatifs(other.text, significatifs)
                            if sig_other and sig_other != sig_noun:
                                relations.append((sig_other, 'RELIE', sig_noun))
                                break

        return relations

    def _extract_relative_relations(self, significatifs: List[str],
                                    doc) -> List[Tuple[str, str, str]]:
        """Extrait les relations des propositions relatives"""
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
        """Extrait les relations à partir des triplets avec relation = 0"""
        relations = []

        for word1, relation, word2 in triplets:
            if str(relation) == '0':
                sig1 = self._find_in_significatifs(word1, significatifs)
                sig2 = self._find_in_significatifs(word2, significatifs)

                if sig1 and sig2 and sig1 != sig2:
                    rel_type = self._analyze_semantic_relation(sig1, sig2)
                    relations.append((sig1, rel_type, sig2))

        return relations

    def _cover_unused_words(self, significatifs: List[str], used: Set[str],
                            triplets: List[Tuple], doc) -> List[Tuple[str, str, str]]:
        """Couvre les mots non utilisés"""
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

    phrases_simples = [
        "Le chat noir dort paisiblement",
        "Marie cuisine un délicieux gâteau au chocolat.",
        "il est malade et il n'est pas gentil.",
        "Sylvain a un chat noir et blanc qui est gentil.",
        "L'enfant dessine un beau paysage avec des crayons colorés.",
        "Sophie écoute de la musique classique dans sa chambre.",
        "Thomas et Sarah dansent et chantent joyeusement.",
        "Le chien aboie mais il n'est pas méchant.",
        "Anna a acheté une robe. Elle est très belle et coûteuse.",
        "Le directeur est arrivé. Il semble fatigué mais content.",
        "Mes parents voyagent beaucoup. Ils visitent l'Italie cette année.",
        "Cette maison appartient à mon oncle. Elle a cent ans.",
        "Le livre que j'ai lu était passionnant. Il raconte une histoire d'amour.",
        "J'adore cuisiner des plats épicés le dimanche.",
        "Il refuse de parler à ses voisins bruyants.",
        "Nous décidons d'acheter une nouvelle voiture électrique.",
        "Elle commence à étudier le japonais à l'université.",
        "Ils essaient de résoudre ce problème mathématique complexe.",
        "La voiture rouge et la moto bleue roulent sur l'autoroute."
    ]

    for phrase in phrases_simples:
        print(ex.extract(phrase))