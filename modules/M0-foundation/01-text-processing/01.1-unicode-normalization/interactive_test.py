#!/usr/bin/env python3
"""
Interactive Text Normalization Test Tool

This script allows you to test the normalizer with any custom text.
You can run it interactively or pass text as command-line arguments.

Usage:
    # Interactive mode
    python interactive_test.py
    
    # Command-line mode
    python interactive_test.py "Your text here"
    python interactive_test.py "سلام دنیا" "Hello World"
    
    # With detailed output
    python interactive_test.py --verbose "Your text"
    
    # Batch test from file
    python interactive_test.py --file texts.txt
"""

import sys
import argparse
from text_processing import normalize_universal


def print_separator(char="=", length=70):
    """Print a separator line."""
    print(char * length)


def normalize_and_display(text: str, verbose: bool = False):
    """
    Normalize text and display results.
    
    Args:
        text: Input text to normalize
        verbose: Show detailed output
    """
    result = normalize_universal(text)
    
    print_separator()
    print(f"📝 INPUT TEXT:")
    print(f"   {text}")
    print()
    print(f"📊 NORMALIZED TEXT:")
    print(f"   {result.text}")
    print()
    print(f"🔍 METADATA:")
    print(f"   Script:          {result.script}")
    print(f"   Original Length: {len(result.original)} characters")
    print(f"   Final Length:    {len(result.text)} characters")
    print(f"   Changes Applied: {len(result.changes)}")
    
    if verbose:
        print()
        print(f"🔧 TRANSFORMATION DETAILS:")
        for i, change in enumerate(result.changes, 1):
            print(f"   {i}. {change}")
    
    print_separator()
    print()


def interactive_mode():
    """Run in interactive mode."""
    print_separator("=")
    print("🎯 INTERACTIVE TEXT NORMALIZATION TEST")
    print_separator("=")
    print()
    print("Enter text to normalize (or 'quit' to exit)")
    print("Commands:")
    print("  - Type any text and press Enter")
    print("  - Type 'verbose' to toggle detailed output")
    print("  - Type 'examples' to see sample texts")
    print("  - Type 'quit' or 'exit' to quit")
    print()
    
    verbose = False
    
    while True:
        try:
            text = input("\n📝 Enter text: ").strip()
            
            if not text:
                continue
            
            if text.lower() in ['quit', 'exit', 'q']:
                print("\n👋 Goodbye!")
                break
            
            if text.lower() == 'verbose':
                verbose = not verbose
                status = "ON" if verbose else "OFF"
                print(f"✅ Verbose mode: {status}")
                continue
            
            if text.lower() == 'examples':
                show_examples()
                continue
            
            normalize_and_display(text, verbose)
            
        except KeyboardInterrupt:
            print("\n\n👋 Interrupted. Goodbye!")
            break
        except Exception as e:
            print(f"❌ Error: {e}")


def show_examples():
    """Show example texts in different languages."""
    examples = {
        "English": "Hello World! This is a test.",
        "Persian": "سلام دنیا! این یک تست است.",
        "Arabic": "مرحبا بالعالم! هذا اختبار.",
        "Chinese": "你好世界！这是一个测试。",
        "Japanese": "こんにちは世界！これはテストです。",
        "Korean": "안녕하세요 세계! 이것은 테스트입니다.",
        "Russian": "Привет мир! Это тест.",
        "Hebrew": "שלום עולם! זה מבחן.",
        "Mixed": "Hello سلام 你好 Привет!",
    }
    
    print()
    print_separator()
    print("📚 EXAMPLE TEXTS:")
    print_separator()
    for lang, text in examples.items():
        print(f"  {lang:10} : {text}")
    print_separator()


def batch_test_from_file(filename: str, verbose: bool = False):
    """
    Test normalization with texts from a file.
    
    Args:
        filename: Path to file containing texts (one per line)
        verbose: Show detailed output
    """
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            texts = [line.strip() for line in f if line.strip()]
        
        print(f"\n📂 Testing {len(texts)} texts from {filename}")
        print()
        
        for i, text in enumerate(texts, 1):
            print(f"\n{'=' * 70}")
            print(f"TEST {i}/{len(texts)}")
            normalize_and_display(text, verbose)
        
        print(f"✅ Completed testing {len(texts)} texts")
        
    except FileNotFoundError:
        print(f"❌ Error: File '{filename}' not found")
    except Exception as e:
        print(f"❌ Error reading file: {e}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Interactive Text Normalization Test Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode
  python interactive_test.py
  
  # Test single text
  python interactive_test.py "Hello World"
  
  # Test multiple texts
  python interactive_test.py "Hello" "سلام" "你好"
  
  # Verbose output
  python interactive_test.py --verbose "Your text"
  
  # Batch test from file
  python interactive_test.py --file texts.txt
        """
    )
    
    parser.add_argument(
        'texts',
        nargs='*',
        help='Text(s) to normalize'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Show detailed transformation information'
    )
    parser.add_argument(
        '-f', '--file',
        type=str,
        help='Read texts from file (one per line)'
    )
    parser.add_argument(
        '-e', '--examples',
        action='store_true',
        help='Show example texts'
    )
    
    args = parser.parse_args()
    
    # Show examples
    if args.examples:
        show_examples()
        return
    
    # Batch test from file
    if args.file:
        batch_test_from_file(args.file, args.verbose)
        return
    
    # Command-line mode with provided texts
    if args.texts:
        for text in args.texts:
            normalize_and_display(text, args.verbose)
        return
    
    # Interactive mode (no arguments provided)
    interactive_mode()


if __name__ == "__main__":
    main()

