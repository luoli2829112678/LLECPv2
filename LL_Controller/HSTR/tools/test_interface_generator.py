"""验证接口清单的错误诊断，防止配置笔误生成错误的调用布局。"""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from generate_interfaces import load_manifest


class ManifestValidation(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = Path(self.folder.name) / "llpc.json"
        self.entry = {"name": "AXIS_ENABLE", "return": "VOID", "callback": "axisEnable",
                      "sdk": "AXIS_Enable", "params": [["axis", "DINT"]]}

    def load(self, entries):
        self.path.write_text(json.dumps({"functions": entries}), encoding="utf-8")
        return load_manifest(self.path)

    def test_invalid_definitions(self):
        invalid = [
            ("return", "STRING"), ("return", "BAD_TYPE"), ("return", []),
            ("callback", "user"), ("callback", "trace"), ("callback", "class"),
            ("name", "IF"), ("name", "HSTR_Internal"), ("name", "AXIS__ENABLE"),
            ("params", [["axis", "VOID"]]), ("params", [["axis", "DINT"], ["AXIS", "DINT"]]),
            ("params", [["axis", "DINT*"]]), ("params", [["AXIS_ENABLE", "DINT"]]),
            ("params", [["axis"]]), ("sdk", "AXIS_Enable(axis)"),
            ("description", "comment\\"), ("aliases", [{"name": "Alias", "params": []}]),
            ("aliases", [{"name": "Alias", "params": [["axis", "LREAL"]]}]),
            ("unexpected", True),
        ]
        for field, value in invalid:
            with self.subTest(field=field, value=value):
                entry = dict(self.entry, **{field: value})
                with self.assertRaises(ValueError):
                    self.load([entry])

    def test_duplicate_function_alias_and_callback(self):
        duplicate = dict(self.entry, name="axis_enable", callback="other")
        with self.assertRaisesRegex(ValueError, "duplicate ST"):
            self.load([self.entry, duplicate])
        duplicate = dict(self.entry, name="OTHER")
        with self.assertRaisesRegex(ValueError, "duplicate callback"):
            self.load([self.entry, duplicate])
        alias = dict(self.entry, aliases=["axis_enable"])
        with self.assertRaisesRegex(ValueError, "duplicate ST"):
            self.load([alias])

    def test_required_field_and_alias_parameter_names(self):
        entry = copy.deepcopy(self.entry)
        del entry["sdk"]
        with self.assertRaisesRegex(ValueError, "missing fields"):
            self.load([entry])
        entry = dict(self.entry, aliases=[{"name": "AxisAlias", "params": [["axisid", "DINT"]]}])
        result = self.load([entry])
        self.assertEqual(result[0]["variants"][1]["params"], [["AXISID", "DINT"]])


if __name__ == "__main__":
    unittest.main()
