"""Unit tests for UmgLayoutCompiler unified layout semantics (no UE required)."""

import unittest

from Bridge.UmgLayoutCompiler import UmgLayoutCompiler, LayoutLint


class TestUnifiedLayoutSemantics(unittest.TestCase):
    def test_size_fill_compiles_to_box_fill(self):
        dsl = {
            "layout_type": "flex",
            "direction": "column",
            "name": "Root",
            "children": [
                {"type": "Text", "name": "Title", "size": "auto"},
                {"type": "Border", "name": "Body", "size": "fill", "fill_weight": 2},
            ],
        }
        compiled, lint = UmgLayoutCompiler.compile(dsl)
        self.assertTrue(lint.ok)
        body = compiled["children"][1]
        self.assertEqual(body["properties"]["Slot"]["Size"]["SizeRule"], "Fill")
        self.assertEqual(body["properties"]["Slot"]["Size"]["Value"], 2.0)
        title = compiled["children"][0]
        self.assertEqual(title["properties"]["Slot"]["Size"]["SizeRule"], "Auto")

    def test_h_align_v_align_on_flex_child(self):
        dsl = {
            "layout_type": "flex",
            "direction": "column",
            "name": "Root",
            "children": [
                {
                    "type": "Button",
                    "name": "Btn",
                    "size": "fill",
                    "h_align": "Center",
                    "v_align": "Center",
                },
            ],
        }
        compiled, _ = UmgLayoutCompiler.compile(dsl)
        slot = compiled["children"][0]["properties"]["Slot"]
        self.assertEqual(slot["HorizontalAlignment"], "Center")
        self.assertEqual(slot["VerticalAlignment"], "Center")

    def test_canvas_size_fill_uses_stretch_anchors(self):
        dsl = {
            "layout_type": "absolute",
            "name": "Canvas",
            "children": [
                {"type": "Border", "name": "Overlay", "size": "fill"},
            ],
        }
        compiled, _ = UmgLayoutCompiler.compile(dsl)
        slot = compiled["children"][0]["properties"]["Slot"]
        anchors = slot["LayoutData"]["Anchors"]
        self.assertEqual(anchors["Minimum"], [0.0, 0.0])
        self.assertEqual(anchors["Maximum"], [1.0, 1.0])

    def test_canvas_h_align_v_align_to_position(self):
        dsl = {
            "layout_type": "absolute",
            "name": "Canvas",
            "children": [
                {"type": "Button", "name": "Ok", "size": "auto", "h_align": "Right", "v_align": "Bottom"},
            ],
        }
        compiled, _ = UmgLayoutCompiler.compile(dsl)
        slot = compiled["children"][0]["properties"]["Slot"]
        anchors = slot["LayoutData"]["Anchors"]
        self.assertEqual(anchors["Minimum"], [1.0, 1.0])
        self.assertEqual(anchors["Maximum"], [1.0, 1.0])

    def test_width_100_percent_maps_to_fill(self):
        dsl = {
            "layout_type": "flex",
            "direction": "row",
            "name": "Row",
            "children": [{"type": "Spacer", "name": "S", "width": "100%"}],
        }
        compiled, _ = UmgLayoutCompiler.compile(dsl)
        slot = compiled["children"][0]["properties"]["Slot"]
        self.assertEqual(slot["Size"]["SizeRule"], "Fill")

    def test_parse_widget_tree_parents(self):
        tree = """WBP_Test [WidgetBlueprint]
  Root [CanvasPanel]
    - MainCol [VerticalBox]
      - Header [Border]
      - Content [Border]
"""
        parents = UmgLayoutCompiler.parse_widget_tree_parents(tree)
        self.assertEqual(parents["MainCol"], "CanvasPanel")
        self.assertEqual(parents["Header"], "VerticalBox")
        self.assertEqual(parents["Content"], "VerticalBox")

    def test_patch_misroute_blocked_on_full_compile(self):
        dsl = {"patch": True, "updates": [{"name": "A", "size": "fill"}]}
        compiled, lint = UmgLayoutCompiler.compile(dsl)
        self.assertFalse(lint.ok)
        self.assertEqual(compiled, {})
        self.assertTrue(any(i.get("ruleId") == "patch-misrouted" for i in lint.issues))
        dsl = {
            "patch": True,
            "updates": [
                {"name": "Content", "size": "fill", "h_align": "Center", "v_align": "Fill"},
            ],
        }
        parent_map = {"Content": "VerticalBox"}
        patches, lint = UmgLayoutCompiler.compile_patch(dsl, parent_map)
        self.assertTrue(lint.ok)
        self.assertEqual(len(patches), 1)
        self.assertEqual(patches[0]["widget_name"], "Content")
        self.assertEqual(patches[0]["slot"]["Size"]["SizeRule"], "Fill")
        self.assertEqual(patches[0]["slot"]["HorizontalAlignment"], "Center")


if __name__ == "__main__":
    unittest.main()
