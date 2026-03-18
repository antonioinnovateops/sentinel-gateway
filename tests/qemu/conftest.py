"""
Pytest configuration for QEMU SIL tests.
"""

import pytest


def pytest_configure(config):
    """Add custom markers."""
    config.addinivalue_line(
        "markers", "qemu: marks tests that require QEMU emulation"
    )


def pytest_collection_modifyitems(config, items):
    """Add qemu marker to all tests in this directory."""
    for item in items:
        if "qemu" in str(item.fspath):
            item.add_marker(pytest.mark.qemu)
