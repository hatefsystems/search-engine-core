# 🎯 Task XX.X: [Task Name]

## 📅 Sprint Info
- **Duration:** [X] days  
- **Milestone:** [MX] - [Milestone Name]  
- **Priority:** [P0/P1/P2]  
- **Depends On:** Task [XX.X] ✅  
- **Blocks:** Task [XX.X]  
- **Assignee:** TBD

## 🎬 What You'll Build
[Brief description of what this task accomplishes - 1-2 sentences]

## 📋 Daily Breakdown

### Day 1: [Focus Area]
- [ ] [Specific deliverable]
- [ ] [Specific deliverable]
- [ ] [Test milestone]

### Day 2: [Focus Area]
- [ ] [Specific deliverable]
- [ ] [Specific deliverable]
- [ ] [Test milestone]

### Day 3+: [Focus Area]
- [ ] [Specific deliverable]
- [ ] [Final testing and validation]

## ✅ Acceptance Criteria
- [ ] [Measurable criterion with metric]
- [ ] [Measurable criterion with metric]
- [ ] [Performance target]
- [ ] [Coverage/quality target]

## 📊 Quality Evidence
- [ ] Baseline comparison attached if this task affects retrieval, ranking, crawling, freshness, spam, semantic expansion, query processing, or click learning
- [ ] Per-bucket results checked for language, script, intent, freshness, and Persian/Iran-local queries where relevant
- [ ] Failure examples documented, not only aggregate metrics
- [ ] Latency/resource impact measured
- [ ] Rollback notes included for deployable ranking, model, indexing, crawling, or cache changes

## 🧪 Testing Checklist
- [ ] Unit tests (≥85% coverage target)
- [ ] Integration tests with dependent tasks
- [ ] Performance benchmarks (`benchmarks/[feature]_perf.py`)
- [ ] Interactive testing tool (`interactive_test.py`)
- [ ] Edge case tests (empty strings, malformed input, special characters)
- [ ] Multi-language validation (if applicable)
- [ ] Persian/Iran-local validation (if applicable)
- [ ] Error handling tests (missing dependencies, invalid input)

## 🎉 Celebration Criteria (Definition of Done)
✅ **Demo Ready:** [What to demonstrate]  
✅ **Metric Met:** [Specific success metric]  
✅ **Integration:** [How it connects to system]  

**🎊 Celebration Moment:** [What makes this exciting!]

## 📦 Deliverables

### Core Implementation Files
- `path/to/main_file.[cpp/py]` ([X] lines estimated)
- `path/to/test_file.[cpp/py]` ([X]+ test cases)
- `shared/logger.py` (Copy from existing tasks or create new structured logger)
- `shared/__init__.py`

### Documentation Files (Required)
- `README.md` - Complete overview, quick start, usage examples, performance metrics
- `QUICK_START.md` - 5-minute setup guide with examples
- `docs/ALGORITHMS.md` - Technical implementation details (see requirements below)
- `docs/api/[feature]-guide.md` - API documentation with examples
- `PROJECT_STATUS.txt` - Completion status with acceptance criteria checklist

### ALGORITHMS.md Requirements
The `docs/ALGORITHMS.md` file must include:
- **Detailed algorithm explanations** with step-by-step breakdowns
- **Mathematical formulations** where applicable
- **Code examples** demonstrating key concepts
- **Performance characteristics** (complexity, optimization strategies)
- **Learning Resources section** with categorized links:
  - Official documentation and specifications
  - Research papers and academic resources
  - Tutorials and hands-on guides
  - Implementation examples and libraries
  - Related concepts and advanced topics
- **References** section with citations

See `modules/M0-foundation/01-text-processing/01.2-language-detection/docs/ALGORITHMS.md` and `01.3-script-specific-processing/docs/ALGORITHMS.md` for examples.

### Testing & Tools
- `tests/test_[feature].py` - Comprehensive test suite (≥85% coverage target)
- `tests/conftest.py` - Pytest fixtures and configuration
- `tests/__init__.py`
- `interactive_test.py` - Interactive CLI for testing (Python tasks)
- `benchmarks/[feature]_perf.py` - Performance benchmarks (Python tasks)

### Configuration Files
- `setup.py` or `pyproject.toml` - Package configuration
  - Use `setup.py` for simple packages
  - Use `pyproject.toml` for modern Python packaging (PEP 518)
  - Include proper metadata (name, version, description, classifiers)
- `requirements.txt` - Runtime dependencies
  - List only production dependencies
  - Pin versions (e.g., `>=0.9.2`)
- `requirements-dev.txt` - Development dependencies
  - Include `-r requirements.txt` at top
  - Add testing, linting, formatting tools
- `pytest.ini` - Test configuration
  - Match pattern from Task 01.2/01.3
  - Include coverage settings
  - Define custom markers (slow, integration, requires_model, etc.)
- `.gitignore` - Git ignore patterns
  - **Required patterns**: `models/`, `training_data/`, `__pycache__/`, `.coverage`, `htmlcov/`, `.benchmarks/`
  - Include IDE files (`.vscode/`, `.idea/`)
  - Include OS files (`.DS_Store`, `Thumbs.db`)
  - Keep directory structure: `!models/.gitkeep`, `!models/README.md`

### Optional Files (As Needed)
- `examples/integration_example.py` - Integration examples
- `scripts/[utility].sh` or `scripts/[utility].py` - Utility scripts
- `docs/TRAINING_GUIDE.md` - Training guides (for ML tasks)
- `models/README.md` - Model documentation (if using models)
- `training_data/README.md` - Training data documentation (if applicable)

## 🔗 Dependencies & Integration

### Input
```
[Data/API/Format expected as input]
```

### Output
```
[Data/API/Format produced as output]
```

## 🚀 Next Steps
➡️ **Task [XX.X]:** [Next task name] ([X] days)  
- [How next task depends on this one]

## 💡 Tips & Resources

### Project Structure Pattern
Follow the established structure from completed tasks:
```
[task-name]/
├── text_processing/          # Main implementation
│   ├── __init__.py
│   └── [main_module].py
├── tests/                    # Test suite
│   ├── __init__.py
│   ├── conftest.py
│   └── test_[feature].py
├── benchmarks/               # Performance tests (Python)
│   ├── __init__.py
│   └── [feature]_perf.py
├── shared/                   # Shared utilities
│   ├── __init__.py
│   └── logger.py            # Structured logging
├── docs/                     # Documentation
│   ├── ALGORITHMS.md
│   └── api/
│       └── [feature]-guide.md
├── examples/                 # Integration examples (optional)
│   └── integration_example.py
├── scripts/                  # Utility scripts (optional)
│   └── [utility].sh or .py
├── models/                   # ML models (if applicable)
│   └── README.md
├── training_data/            # Training data (if applicable)
│   └── README.md
├── README.md                 # Main documentation
├── QUICK_START.md           # Quick start guide
├── PROJECT_STATUS.txt       # Completion status
├── interactive_test.py      # Interactive testing tool
├── setup.py                 # Package configuration
├── pyproject.toml          # Alternative to setup.py (optional)
├── requirements.txt         # Runtime dependencies
├── requirements-dev.txt    # Development dependencies
├── pytest.ini             # Test configuration
└── .gitignore              # Git ignore patterns
```

### Common Pitfalls
- ⚠️ [Pitfall 1]: [How to avoid]
- ⚠️ [Pitfall 2]: [How to avoid]
- ⚠️ **Missing shared/logger.py**: Always copy from existing tasks or create structured logger
- ⚠️ **No interactive_test.py**: Create interactive CLI for easy testing
- ⚠️ **Missing benchmarks**: Include performance benchmarks for Python tasks
- ⚠️ **Incomplete .gitignore**: Include models/, training_data/, __pycache__/, .coverage, htmlcov/

### Helpful Resources
- [Resource 1 with link]
- [Resource 2 with link]
- **Reference completed tasks**: Check `01.1-unicode-normalization` and `01.2-language-detection` for patterns
- **Shared logger**: Reuse `shared/logger.py` from existing tasks

### Example Code (if applicable)
```language
// Quick example showing key concept
```

### Documentation Standards

**README.md** must include:
- Status section: `## ✅ Status: COMPLETE & PRODUCTION-READY`
- Performance metrics table (Target vs Achieved)
- Quick start section with code examples
- Project structure diagram
- Integration examples
- Troubleshooting section

**QUICK_START.md** format:
- 5-minute setup guide
- Copy-paste examples
- Common tasks section
- Troubleshooting tips
- Reference to full README.md

**PROJECT_STATUS.txt** format:
- ASCII art header (see Task 01.1/01.2 examples)
- Acceptance criteria checklist
- Test results summary
- Performance benchmarks table
- Deliverables checklist

**ALGORITHMS.md**: See requirements above - must match depth of Task 01.2/01.3 examples

### Code Quality Standards
- **Type hints**: Use type hints for all functions
- **Docstrings**: All public APIs must have docstrings
- **Error handling**: Graceful degradation, never crash on malformed input
- **Logging**: Use structured logging (`shared/logger.py`) instead of `print()` or `std::cout`
- **Testing**: ≥85% coverage target, comprehensive edge case tests

## 📊 Success Metrics
- **Quality:** [Metric]
- **Performance:** [Metric]
- **Coverage:** [Metric]

## 🎓 Learning Outcomes
After completing this task, you will:
- ✅ [Skill/knowledge gained]
- ✅ [Skill/knowledge gained]
- ✅ [Skill/knowledge gained]

---

**[Motivational tagline!] 🚀**
