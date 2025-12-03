import unittest
from unittest.mock import MagicMock
import sys
import os

# Add current directory to path to find UMGSequencer
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from UMGSequencer import UMGSequencer

class TestUMGSequencer(unittest.TestCase):
    def setUp(self):
        self.mock_conn = MagicMock()
        self.sequencer = UMGSequencer(self.mock_conn)

    def test_get_all_animations(self):
        self.sequencer.get_all_animations("/Game/Tests/MyWidget")
        self.mock_conn.send_command.assert_called_with("get_all_animations", {"asset_path": "/Game/Tests/MyWidget"})

    def test_create_animation(self):
        self.sequencer.create_animation("/Game/Tests/MyWidget", "NewAnim")
        self.mock_conn.send_command.assert_called_with("create_animation", {
            "asset_path": "/Game/Tests/MyWidget",
            "animation_name": "NewAnim"
        })

    def test_delete_animation(self):
        self.sequencer.delete_animation("/Game/Tests/MyWidget", "OldAnim")
        self.mock_conn.send_command.assert_called_with("delete_animation", {
            "asset_path": "/Game/Tests/MyWidget",
            "animation_name": "OldAnim"
        })

    def test_add_track(self):
        self.sequencer.add_track("/Game/Tests/MyWidget", "Anim1", "Button_0", "RenderOpacity")
        self.mock_conn.send_command.assert_called_with("add_track", {
            "asset_path": "/Game/Tests/MyWidget",
            "animation_name": "Anim1",
            "widget_name": "Button_0",
            "property_name": "RenderOpacity"
        })

    def test_add_key(self):
        self.sequencer.add_key("/Game/Tests/MyWidget", "Anim1", "Button_0", "RenderOpacity", 0.5, 1.0)
        self.mock_conn.send_command.assert_called_with("add_key", {
            "asset_path": "/Game/Tests/MyWidget",
            "animation_name": "Anim1",
            "widget_name": "Button_0",
            "property_name": "RenderOpacity",
            "time": 0.5,
            "value": 1.0
        })

if __name__ == '__main__':
    unittest.main()
