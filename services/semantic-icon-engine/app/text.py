"""Versioned normalization without destructive stemming or runtime translation."""
import json
import re
import unicodedata
from pathlib import Path
CONFIG = Path(__file__).resolve().parents[1] / 'config'
VERSION = 'fa-en-v2'
SYNONYMS = json.loads((CONFIG / 'synonyms.json').read_text())

def normalize(text):
    text = unicodedata.normalize('NFKC', text).lower().translate(str.maketrans('يك٠١٢٣٤٥٦٧٨٩۰۱۲۳۴۵۶۷۸۹', 'یک01234567890123456789'))
    text = ''.join(' ' if unicodedata.category(c) == 'Cf' else c for c in text if not ('\u0610' <= c <= '\u061a' or '\u064b' <= c <= '\u065f' or c == '\u0670' or '\u06d6' <= c <= '\u06ed'))
    return ' '.join(re.findall(r'[^\W_]+', text))

def terms(text):
    return set(normalize(text).split())

def categories(text):
    value = ' ' + normalize(text) + ' '
    return [k for k, v in SYNONYMS['categories'].items() if any(' ' + normalize(a) + ' ' in value for a in v['aliases'])]

def expand(text):
    return normalize(text + ' ' + ' '.join(' '.join(SYNONYMS['categories'][k]['aliases']) for k in categories(text)))

def enrich(name, tags):
    cats = categories(name.replace('-', ' ') + ' ' + ' '.join(tags))
    return cats, ' '.join(SYNONYMS['categories'][c]['fa'] for c in cats)
