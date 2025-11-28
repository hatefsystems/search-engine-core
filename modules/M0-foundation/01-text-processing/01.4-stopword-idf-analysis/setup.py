"""Setup configuration for stopword IDF analysis package - Task 01.4"""

from setuptools import setup, find_packages
from pathlib import Path

# Read README
readme_file = Path(__file__).parent / "README.md"
if readme_file.exists():
    with open(readme_file, "r", encoding="utf-8") as fh:
        long_description = fh.read()
else:
    long_description = "Universal automatic stopword detection using IDF analysis"

setup(
    name="search-engine-stopword-idf",
    version="0.1.0",
    author="Search Engine Team",
    description="Universal automatic stopword detection using IDF analysis - supports 100+ languages",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(exclude=["tests", "benchmarks", "scripts"]),
    python_requires=">=3.9",
    install_requires=[
        "numpy>=1.24.0,<2.0",
        "scipy>=1.11.0",
        "pymongo>=4.6.0",
        "redis>=5.0.0",
        "structlog>=23.2.0",
        "pydantic>=2.5.0",
        "nltk>=3.8.1",
    ],
    extras_require={
        "dev": [
            "pytest>=7.4.3",
            "pytest-cov>=4.1.0",
            "pytest-benchmark>=4.0.0",
            "pytest-timeout>=2.2.0",
            "pytest-mock>=3.12.0",
            "black>=23.12.0",
            "flake8>=6.1.0",
            "mypy>=1.7.1",
            "isort>=5.13.2",
            "memory-profiler>=0.61.0",
            "line-profiler>=4.1.0",
        ],
    },
    entry_points={
        "console_scripts": [
            "compute-stopwords=scripts.compute_idf_batch:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Text Processing :: Linguistic",
        "Topic :: Scientific/Engineering :: Information Analysis",
    ],
    keywords="stopwords idf nlp search-engine information-retrieval",
)

