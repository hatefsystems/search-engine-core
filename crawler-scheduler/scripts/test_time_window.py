#!/usr/bin/env python3
"""
Test time window logic for crawler scheduler
Validates that end hours are inclusive and edge cases work correctly
"""

import sys
from datetime import datetime, time

# Mock Config for testing
class MockConfig:
    WARMUP_ENABLED = True
    WARMUP_START_HOUR = 0
    WARMUP_END_HOUR = 23
    
    @classmethod
    def get_warmup_schedule(cls):
        return [50, 100, 200, 400, 800]

def test_time_window(current_hour, start_hour, end_hour):
    """Test the time window logic"""
    # Replicate the logic from rate_limiter.py
    if end_hour == 0 or end_hour == 24:
        end_hour = 24
    
    if start_hour <= end_hour:
        # Normal case
        in_window = start_hour <= current_hour <= end_hour
    else:
        # Wrap-around case
        in_window = current_hour >= start_hour or current_hour <= end_hour
    
    return in_window

def run_tests():
    """Run comprehensive time window tests"""
    print("=" * 70)
    print("Time Window Logic Tests")
    print("=" * 70)
    print()
    
    test_cases = [
        # (description, current_hour, start_hour, end_hour, expected_result)
        # Normal case: 0-23 (full day)
        ("Full day (0-23), hour 0", 0, 0, 23, True),
        ("Full day (0-23), hour 12", 12, 0, 23, True),
        ("Full day (0-23), hour 23", 23, 0, 23, True),  # This is the fix!
        
        # Normal case: 10-12 (2 hour window)
        ("Window 10-12, hour 9", 9, 10, 12, False),
        ("Window 10-12, hour 10", 10, 10, 12, True),
        ("Window 10-12, hour 11", 11, 10, 12, True),
        ("Window 10-12, hour 12", 12, 10, 12, True),  # End hour inclusive
        ("Window 10-12, hour 13", 13, 10, 12, False),
        
        # Edge case: End of day with hour 24
        ("End of day (0-24), hour 23", 23, 0, 24, True),
        ("End of day (0-24), hour 0", 0, 0, 24, True),
        
        # Edge case: Wrap-around (22-2)
        ("Wrap-around (22-2), hour 21", 21, 22, 2, False),
        ("Wrap-around (22-2), hour 22", 22, 22, 2, True),
        ("Wrap-around (22-2), hour 23", 23, 22, 2, True),
        ("Wrap-around (22-2), hour 0", 0, 22, 2, True),
        ("Wrap-around (22-2), hour 1", 1, 22, 2, True),
        ("Wrap-around (22-2), hour 2", 2, 22, 2, True),
        ("Wrap-around (22-2), hour 3", 3, 22, 2, False),
        
        # Edge case: Single hour window
        ("Single hour (10-10), hour 9", 9, 10, 10, False),
        ("Single hour (10-10), hour 10", 10, 10, 10, True),
        ("Single hour (10-10), hour 11", 11, 10, 10, False),
    ]
    
    passed = 0
    failed = 0
    
    for description, current_hour, start_hour, end_hour, expected in test_cases:
        result = test_time_window(current_hour, start_hour, end_hour)
        status = "✓ PASS" if result == expected else "✗ FAIL"
        
        if result == expected:
            passed += 1
        else:
            failed += 1
        
        # Format display
        end_display = f"{end_hour}:59" if end_hour not in [0, 24] else "23:59"
        if end_hour == 24:
            end_display = "23:59"
        
        print(f"{status} | {description}")
        print(f"       Current: {current_hour}:00, Window: {start_hour}:00-{end_display}")
        print(f"       Result: {result}, Expected: {expected}")
        
        if result != expected:
            print(f"       ❌ MISMATCH!")
        
        print()
    
    print("=" * 70)
    print(f"Test Results: {passed} passed, {failed} failed")
    print("=" * 70)
    
    if failed > 0:
        print("\n❌ Some tests failed!")
        return False
    else:
        print("\n✅ All tests passed!")
        return True

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)

