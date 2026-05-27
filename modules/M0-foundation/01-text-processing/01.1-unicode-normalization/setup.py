"""Setup configuration for ml-pipeline package."""

from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="search-engine-ml-pipeline",
    version="0.1.0",
    author="Search Engine Team",
    description="ML/NLP pipeline for multilingual search engine",
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(exclude=["tests", "benchmarks"]),
    python_requires=">=3.9",
    install_requires=[
        "pyicu>=2.11",
        "pydantic>=2.5.0",
        "python-dotenv>=1.0.0",
        "fastapi>=0.104.1",
        "uvicorn>=0.24.0",
        "httpx>=0.25.2",
        "structlog>=23.2.0",
    ],
    extras_require={
        "dev": [
            "pytest>=7.4.3",
            "pytest-cov>=4.1.0",
            "pytest-benchmark>=4.0.0",
            "black>=23.12.0",
            "flake8>=6.1.0",
            "mypy>=1.7.1",
            "isort>=5.13.2",
            "memory-profiler>=0.61.0",
        ],
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
)

