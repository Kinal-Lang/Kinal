# Testing Guide

## Running Tests

```bash
# Run all tests
python x.py test

# Run a single test
python x.py test tests/arrays.kn
```

## Test Files

- Located in the `tests/` directory
- File extension: `.kn`
- Files prefixed with `error_` are **negative tests** that are expected to fail compilation — they check for specific error messages

## Writing Tests

1. Create a `.kn` file under `tests/`
2. Positive tests: Write a normal program that is expected to compile and run successfully
3. Negative tests: Name the file with the `error_` prefix and annotate expected error messages inside the file
