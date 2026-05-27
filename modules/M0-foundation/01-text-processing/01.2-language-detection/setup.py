"""Setup configuration for language detection package - Task 01.2"""

from setuptools import setup, find_packages
from pathlib import Path

# Read README
readme_file = Path(__file__).parent / "README.md"
if readme_file.exists():
    with open(readme_file, "r", encoding="utf-8") as fh:
        long_description = fh.read()
else:
    long_description = "Universal language detection for search engine"

setup(
    name="search-engine-language-detection",
    version="0.1.0",
    author="Search Engine Team",
    description="Universal language detection supporting 176+ languages, scalable to 250+",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(exclude=["tests", "benchmarks", "scripts"]),
    python_requires=">=3.9",
    install_requires=[
        "fasttext-wheel>=0.9.2",
        "langdetect>=1.0.9",
        "structlog>=23.2.0",
        "pydantic>=2.5.0",
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
            # Optional extended language detection
            # "polyglot>=16.7.4",  # Requires ICU installation
            # "pycld2>=0.41",
        ],
    },
    entry_points={
        "console_scripts": [
            "train-language-model=scripts.train_custom_model:main",
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
    keywords="language-detection multilingual nlp fasttext search-engine",
)

