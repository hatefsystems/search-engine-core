"""Setup configuration for script-specific processing package - Task 01.3"""

from setuptools import setup, find_packages
from pathlib import Path

# Read README
readme_file = Path(__file__).parent / "README.md"
if readme_file.exists():
    with open(readme_file, "r", encoding="utf-8") as fh:
        long_description = fh.read()
else:
    long_description = "Script-specific text processing for search engine"

setup(
    name="search-engine-script-processing",
    version="0.1.0",
    author="Search Engine Team",
    description="Script-specific text processing: Arabic (ZWNJ), CJK (segmentation), Cyrillic (variants), Latin (diacritics)",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(exclude=["tests", "benchmarks", "scripts", "examples"]),
    python_requires=">=3.9",
    install_requires=[
        "jieba-fast>=0.53.0",  # Fast Chinese word segmentation
        "structlog>=23.2.0",   # Structured logging
        "unicodedata2>=15.1.0",  # Enhanced Unicode support (optional but recommended)
    ],
    extras_require={
        "dev": [
            "pytest>=7.4.3",
            "pytest-cov>=4.1.0",
            "pytest-benchmark>=4.0.0",
            "pytest-timeout>=2.2.0",
            "black>=23.12.0",
            "flake8>=6.1.0",
            "mypy>=1.7.1",
            "isort>=5.13.2",
            "memory-profiler>=0.61.0",
        ],
        "extended": [
            # Optional extended CJK support
            "mecab-python3>=1.0.6",  # Japanese tokenization (requires MeCab system library)
            "opencc-python-reimplemented>=1.1.6",  # Traditional ↔ Simplified Chinese conversion
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Text Processing :: Linguistic",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
    ],
    keywords="text-processing script-processing arabic cjk cyrillic latin unicode nlp search-engine",
)
