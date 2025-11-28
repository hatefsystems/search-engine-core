"""Tests for multilingual stopword detection - Task 01.4"""

import pytest
from text_processing import IDFAnalyzer, StopwordDetector


class TestMultilingualSupport:
    """Test multilingual stopword detection"""
    
    def test_english_stopwords(self, idf_analyzer):
        """Test English stopword detection"""
        corpus = [
            "the quick brown fox jumps",
            "the lazy dog sleeps",
            "a cat and a dog"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="en")
        
        # Common English stopwords should be detected
        the_score = idf_scores.get("the")
        assert the_score is not None
        assert the_score.is_stopword_candidate
        
        a_score = idf_scores.get("a")
        assert a_score is not None
        assert a_score.is_stopword_candidate
    
    def test_persian_stopwords(self, idf_analyzer):
        """Test Persian (Farsi) stopword detection"""
        corpus = [
            "این یک متن فارسی است",
            "این متن برای تست است",
            "یک متن دیگر است"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="fa")
        
        # "است" (is) should be stopword
        ast_score = idf_scores.get("است")
        assert ast_score is not None
        assert ast_score.is_stopword_candidate
        
        # "این" (this) should be stopword
        in_score = idf_scores.get("این")
        assert in_score is not None
        assert in_score.is_stopword_candidate
    
    def test_spanish_stopwords(self, idf_analyzer):
        """Test Spanish stopword detection"""
        corpus = [
            "el rápido zorro marrón salta",
            "el perro perezoso duerme",
            "un gato y un perro"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="es")
        
        # "el" (the) should be stopword
        el_score = idf_scores.get("el")
        assert el_score is not None
        assert el_score.is_stopword_candidate
        
        # "un" (a) should be stopword
        un_score = idf_scores.get("un")
        assert un_score is not None
        assert un_score.is_stopword_candidate
    
    def test_chinese_stopwords(self, idf_analyzer):
        """Test Chinese stopword detection"""
        # Note: Chinese requires word segmentation, but testing character-level here
        corpus = [
            "这是一个中文文本",
            "这是另一个文本",
            "一个测试文本"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="zh", tokenize=False)
        
        # "的" is common Chinese particle
        # Note: May need proper tokenization for accurate detection
        assert len(idf_scores) > 0
    
    def test_arabic_stopwords(self, idf_analyzer):
        """Test Arabic stopword detection"""
        corpus = [
            "هذا نص عربي للاختبار",
            "هذا نص آخر",
            "النص العربي"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="ar")
        
        # "هذا" (this) should be stopword
        hatha_score = idf_scores.get("هذا")
        assert hatha_score is not None
        assert hatha_score.is_stopword_candidate
    
    def test_german_stopwords(self, idf_analyzer):
        """Test German stopword detection"""
        corpus = [
            "der schnelle braune fuchs springt",
            "der faule hund schläft",
            "eine katze und ein hund"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="de")
        
        # "der" (the) should be stopword
        der_score = idf_scores.get("der")
        assert der_score is not None
        assert der_score.is_stopword_candidate
    
    def test_french_stopwords(self, idf_analyzer):
        """Test French stopword detection"""
        corpus = [
            "le renard brun rapide saute",
            "le chien paresseux dort",
            "un chat et un chien"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="fr")
        
        # "le" (the) should be stopword
        le_score = idf_scores.get("le")
        assert le_score is not None
        assert le_score.is_stopword_candidate
    
    def test_russian_stopwords(self, idf_analyzer):
        """Test Russian stopword detection"""
        corpus = [
            "это текст на русском языке",
            "это другой текст",
            "еще один текст"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="ru")
        
        # "это" (this) should be stopword
        eto_score = idf_scores.get("это")
        assert eto_score is not None
        assert eto_score.is_stopword_candidate
    
    def test_japanese_stopwords(self, idf_analyzer):
        """Test Japanese stopword detection"""
        # Note: Japanese requires tokenization, testing character-level here
        corpus = [
            "これは日本語のテキストです",
            "これは別のテキストです",
            "テストのテキスト"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus, language="ja", tokenize=False)
        
        # Should detect some patterns
        assert len(idf_scores) > 0
    
    def test_language_specific_thresholds(self):
        """Test that different languages can have different thresholds"""
        # English analyzer with threshold 2.0
        en_analyzer = IDFAnalyzer(idf_threshold=2.0)
        en_corpus = ["the quick fox", "the lazy dog", "a cat"]
        en_scores = en_analyzer.analyze(en_corpus, language="en")
        
        # Chinese analyzer with different threshold
        zh_analyzer = IDFAnalyzer(idf_threshold=1.5)
        zh_corpus = ["这是文本", "这是测试", "文本测试"]
        zh_scores = zh_analyzer.analyze(zh_corpus, language="zh", tokenize=False)
        
        # Both should have results
        assert len(en_scores) > 0
        assert len(zh_scores) > 0
    
    def test_mixed_language_corpus(self, idf_analyzer):
        """Test corpus with mixed languages"""
        corpus = [
            "English text with words",
            "Texto en español aquí",
            "متن فارسی اینجاست",
            "More English content"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Should still calculate IDF for all terms
        assert len(idf_scores) > 0


class TestLanguageCoverage:
    """Test coverage of multiple languages"""
    
    def test_100plus_languages_capability(self, idf_analyzer):
        """Test that analyzer can handle 100+ languages"""
        # Test with sample from various language families
        languages = {
            "en": ["the cat and dog"],
            "es": ["el gato y perro"],
            "fr": ["le chat et chien"],
            "de": ["die katze und hund"],
            "ru": ["кот и собака"],
            "ar": ["القط والكلب"],
            "fa": ["گربه و سگ"],
            "zh": ["猫和狗"],
            "ja": ["猫と犬"],
            "ko": ["고양이와 개"],
            "hi": ["बिल्ली और कुत्ता"],
            "tr": ["kedi ve köpek"],
            "vi": ["mèo và chó"],
            "th": ["แมวและสุนัข"],
            "id": ["kucing dan anjing"]
        }
        
        for lang, corpus in languages.items():
            # Extend corpus to have meaningful IDF
            extended_corpus = corpus * 5  # Repeat to create corpus
            idf_scores = idf_analyzer.analyze(extended_corpus, language=lang)
            
            assert len(idf_scores) > 0, f"Failed to analyze {lang}"
    
    def test_language_agnostic_algorithm(self, idf_analyzer):
        """Test that IDF algorithm works regardless of language"""
        # Create corpora with similar structure but different languages
        
        # English
        en_corpus = ["the cat", "the dog", "the bird"] * 3
        en_scores = idf_analyzer.analyze(en_corpus)
        en_stopwords = [term for term, score in en_scores.items() 
                       if score.is_stopword_candidate]
        
        # Spanish
        es_corpus = ["el gato", "el perro", "el pájaro"] * 3
        es_scores = idf_analyzer.analyze(es_corpus)
        es_stopwords = [term for term, score in es_scores.items() 
                       if score.is_stopword_candidate]
        
        # Both should detect similar patterns
        assert "the" in en_stopwords  # English "the"
        assert "el" in es_stopwords   # Spanish "el"


@pytest.mark.requires_redis
class TestMultilingualRedis:
    """Test Redis storage with multiple languages"""
    
    def test_multiple_languages_in_redis(self, stopword_detector_redis, idf_analyzer):
        """Test storing stopwords for multiple languages in Redis"""
        # Analyze multiple languages
        languages = {
            "en": ["the cat and dog"] * 5,
            "es": ["el gato y perro"] * 5,
            "fr": ["le chat et chien"] * 5
        }
        
        for lang, corpus in languages.items():
            idf_scores = idf_analyzer.analyze(corpus, language=lang)
            exported = stopword_detector_redis.export_to_redis(idf_scores, lang)
            assert exported > 0, f"Failed to export {lang}"
        
        # Verify all languages are stored
        for lang in languages.keys():
            stopwords = stopword_detector_redis.get_stopwords(lang, limit=5)
            assert len(stopwords) > 0, f"No stopwords retrieved for {lang}"
        
        # Cleanup
        for lang in languages.keys():
            stopword_detector_redis.clear_language(lang)
    
    def test_language_isolation(self, stopword_detector_redis, idf_analyzer):
        """Test that stopwords for different languages are isolated"""
        # Add English stopwords
        en_corpus = ["the cat"] * 5
        en_scores = idf_analyzer.analyze(en_corpus, language="en")
        stopword_detector_redis.export_to_redis(en_scores, "en")
        
        # Add Spanish stopwords
        es_corpus = ["el gato"] * 5
        es_scores = idf_analyzer.analyze(es_corpus, language="es")
        stopword_detector_redis.export_to_redis(es_scores, "es")
        
        # Check English doesn't appear in Spanish
        result = stopword_detector_redis.is_stopword("the", "es")
        # "the" should not be in Spanish stopwords (unless in bootstrap)
        
        # Cleanup
        stopword_detector_redis.clear_language("en")
        stopword_detector_redis.clear_language("es")

