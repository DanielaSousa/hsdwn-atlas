# test_filter_and_sort_f_keys.py
import pytest

def filter_and_sort_f_keys(paths_in_nodes, priority_threshold):
    """
    1. Remove f_keys with priority lower than priority_threshold.
    2. Sort by priority (higher value first).
    3. Sort by dataRate (higher is better).
    4. Sort by the length of lists in paths_in_nodes (descending order).

    :param paths_in_nodes: Dictionary mapping f_keys to lists of nodes.
    :param priority_threshold: Integer threshold for filtering and sorting priorities.
    :return: A sorted list of f_keys.
    """
    # Filter out f_keys with priority lower than the threshold
    filtered_f_keys = [
        f_key for f_key in paths_in_nodes.keys()
        if f_key.priority >= priority_threshold
    ]

    # Sort based on new criteria
    return sorted(
        filtered_f_keys,
        key=lambda f_key: (
            -f_key.priority,  # Higher priority first
            -f_key.get_dataRate(),  # Higher data rate first
            -len(paths_in_nodes[f_key])  # More nodes first
        )
    )

class MockFKey:
    def __init__(self, name, priority, data_rate):
        self.name = name
        self.priority = priority
        self._data_rate = data_rate

    def get_dataRate(self):
        return self._data_rate

    def __repr__(self):
        return f"MockFKey({self.name})"

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other):
        return isinstance(other, MockFKey) and self.name == other.name


def test_filters_by_priority_threshold():
    f1 = MockFKey("f1", priority=1, data_rate=10)
    f2 = MockFKey("f2", priority=3, data_rate=20)

    paths_in_nodes = {
        f1: [1, 2, 3],
        f2: [1]
    }

    result = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=2)

    assert result == [f2]


def test_sorts_by_path_length_descending():
    f1 = MockFKey("f1", priority=1, data_rate=10)
    f2 = MockFKey("f2", priority=1, data_rate=20)

    paths_in_nodes = {
        f1: [1, 2],
        f2: [1, 2, 3, 4]
    }

    result = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=0)

    assert result == [f2, f1]


def test_sorts_by_priority_after_path_length():
    f1 = MockFKey("f1", priority=2, data_rate=10)
    f2 = MockFKey("f2", priority=1, data_rate=10)

    paths_in_nodes = {
        f1: [1, 2, 3],
        f2: [4, 5, 6]
    }

    result = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=0)

    assert result == [f1, f2]


def test_sorts_by_data_rate_last():
    f1 = MockFKey("f1", priority=1, data_rate=50)
    f2 = MockFKey("f2", priority=1, data_rate=100)

    paths_in_nodes = {
        f1: [1, 2, 3],
        f2: [4, 5, 6]
    }

    result = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=0)

    assert result == [f2, f1]


def test_all_sorting_criteria_combined():
    f1 = MockFKey("f1", priority=1, data_rate=50)
    f2 = MockFKey("f2", priority=2, data_rate=100)
    f3 = MockFKey("f3", priority=1, data_rate=10)

    paths_in_nodes = {
        f1: [1, 2, 3, 4],
        f2: [1, 2, 3, 4],
        f3: [1, 2]
    }

    result = filter_and_sort_f_keys(paths_in_nodes, priority_threshold=0)

    assert result == [f2, f1, f3]
