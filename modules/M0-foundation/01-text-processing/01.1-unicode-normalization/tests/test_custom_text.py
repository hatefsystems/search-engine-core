"""
Custom Text Testing - Parameterized Tests

This module allows you to test normalization with your own custom text.
You can run it with pytest or use the interactive test function.
"""

import pytest
from text_processing import normalize_universal


# Parameterized test with custom texts
@pytest.mark.parametrize("text,expected_script,description", [
    # Add your custom test cases here
    ("Hello World", "Latn", "English text"),
    ("سلام دنیا", "Arab", "Persian text"),
    ("你好世界", "Hans", "Chinese text"),
    ("Привет мир", "Cyrl", "Russian text"),
    ("مرحبا بالعالم", "Arab", "Arabic text"),
    ("שלום עולם", "Hebr", "Hebrew text"),
    ("こんにちは", "Jpan", "Japanese text"),
    ("안녕하세요", "Kore", "Korean text"),
    ("Hello سلام 你好", None, "Mixed script text"),  # None = any script
])
def test_custom_text_parametrized(text, expected_script, description):
    """
    Test normalization with custom parametrized text.
    
    Usage:
        pytest tests/test_custom_text.py -v
    """
    result = normalize_universal(text)
    
    # Basic assertions
    assert result.text is not None
    assert len(result.text) > 0
    assert result.original == text
    
    # Check script if specified
    if expected_script:
        assert result.script == expected_script, \
            f"Expected script {expected_script}, got {result.script}"
    
    # Print results for visibility
    print(f"\n  Text: {text}")
    print(f"  Description: {description}")
    print(f"  Script: {result.script}")
    print(f"  Normalized: {result.text}")
    print(f"  Changes: {len(result.changes)}")


def test_your_custom_text():
    """
    Interactive test for any custom text.
    
    Modify the 'custom_texts' list below to test your own text.
    
    Usage:
        pytest tests/test_custom_text.py::test_your_custom_text -v -s
    """
    # ⭐ ADD YOUR CUSTOM TEXTS HERE ⭐
    custom_texts = [
        "Your custom text here",
        "متن دلخواه شما اینجا",
        "任意文本在这里",
        "Ваш текст здесь",
    ]
    
    print("\n" + "=" * 70)
    print("TESTING YOUR CUSTOM TEXTS")
    print("=" * 70)
    
    for i, text in enumerate(custom_texts, 1):
        result = normalize_universal(text)
        
        print(f"\n📝 Test {i}:")
        print(f"   Original:    {text}")
        print(f"   Normalized:  {result.text}")
        print(f"   Script:      {result.script}")
        print(f"   Changes:     {len(result.changes)} transformations")
        
        # Print detailed changes
        if result.changes:
            print(f"   Details:")
            for change in result.changes[:3]:  # Show first 3 changes
                print(f"     • {change}")
        
        # Assertions
        assert result.text is not None
        assert result.original == text
    
    print("\n" + "=" * 70)
    print("✅ All custom texts tested successfully!")
    print("=" * 70)


def test_single_text():
    """
    Test a single piece of text.
    
    Modify the 'text' variable below to test different inputs.
    
    Usage:
        pytest tests/test_custom_text.py::test_single_text -v -s
    """
    # ⭐ CHANGE THIS TEXT TO TEST DIFFERENT INPUTS ⭐
    text = "Hello World! This is a test."
    
    print("\n" + "=" * 70)
    print("SINGLE TEXT TEST")
    print("=" * 70)
    
    result = normalize_universal(text)
    
    print(f"\n📝 Input Text:")
    print(f"   {text}")
    print(f"\n📊 Results:")
    print(f"   Normalized Text:  {result.text}")
    print(f"   Detected Script:  {result.script}")
    print(f"   Original Length:  {len(result.original)} chars")
    print(f"   Normalized Length: {len(result.text)} chars")
    print(f"   Number of Changes: {len(result.changes)}")
    
    print(f"\n🔍 Transformation Details:")
    for i, change in enumerate(result.changes, 1):
        print(f"   {i}. {change}")
    
    print("\n" + "=" * 70)
    print("✅ Test completed successfully!")
    print("=" * 70)
    
    # Assertions
    assert result is not None
    assert result.text is not None
    assert result.original == text


class TestCustomInputs:
    """
    Class-based tests for custom text inputs.
    
    Usage:
        pytest tests/test_custom_text.py::TestCustomInputs -v -s
    """
    
    def test_english_sentences(self):
        """Test various English sentences."""
        texts = [
            "The quick brown fox jumps over the lazy dog.",
            "Hello, World! How are you today?",
            "This is a test of the normalization system.",
        ]
        
        for text in texts:
            result = normalize_universal(text)
            assert len(result.text) > 0
            print(f"\n✅ {text[:50]}... → {result.script}")
    
    def test_persian_sentences(self):
        """Test various Persian sentences."""
        texts = [
            "سلام دنیای زیبا!",
            "این یک متن فارسی است.",
            "تست نرمال‌سازی یونیکد",
        ]
        
        for text in texts:
            result = normalize_universal(text)
            assert result.script == "Arab"
            print(f"\n✅ {text} → {result.script}")
    
    def test_mixed_languages(self):
        """Test mixed language texts."""
        texts = [
            "Hello سلام",
            "Test تست",
            "World 世界 دنیا",
        ]
        
        for text in texts:
            result = normalize_universal(text)
            assert len(result.text) > 0
            print(f"\n✅ {text} → {result.script}")


# Utility function for manual testing
def normalize_text(text: str, show_details: bool = True):
    """
    Utility function to normalize any text and show results.
    
    Args:
        text: Input text to normalize
        show_details: Whether to show detailed results
        
    Returns:
        NormalizedText object
        
    Example:
        >>> from tests.test_custom_text import normalize_text
        >>> result = normalize_text("Hello World")
    """
    result = normalize_universal(text)
    
    if show_details:
        print(f"\n{'=' * 60}")
        print(f"INPUT:  {text}")
        print(f"OUTPUT: {result.text}")
        print(f"SCRIPT: {result.script}")
        print(f"CHANGES: {len(result.changes)}")
        for i, change in enumerate(result.changes, 1):
            print(f"  {i}. {change}")
        print(f"{'=' * 60}")
    
    return result


if __name__ == "__main__":
    # Run interactive test when executed directly
    print("\n" + "=" * 70)
    print("INTERACTIVE TEXT NORMALIZATION TEST")
    print("=" * 70)
    
    # Example usage
    test_texts = [
        "Hello World",
        "سلام دنیا",
        "你好世界",
        "Mixed: Hello سلام 你好",
    ]
    
    for text in test_texts:
        normalize_text(text)

