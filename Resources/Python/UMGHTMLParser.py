import xml.etree.ElementTree as ET
import json
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("UMGHTMLParser")

class UMGHTMLParser:
    """
    Parses HTML/XML-like strings into UMG JSON format.
    """

    def __init__(self):
        pass

    def parse(self, html_content: str) -> Dict[str, Any]:
        """
        Parses the HTML content and returns a JSON structure compatible with 'apply_json_to_umg'.
        
        Args:
            html_content: A string containing the XML/HTML definition of the widget tree.
                          Example:
                          <CanvasPanel>
                              <Button Name="MyBtn" Slot.Position="(100, 100)">
                                  <TextBlock Text="Click Me"/>
                              </Button>
                          </CanvasPanel>
        
        Returns:
            A dictionary representing the UMG widget tree.
        """
        try:
            # Wrap in a root element if multiple roots or just for safety
            # But UMG usually has one root. Let's assume the user provides a single root or we wrap it.
            # For now, assume single root.
            root = ET.fromstring(html_content)
            return self._element_to_json(root)
        except ET.ParseError as e:
            logger.error(f"Failed to parse HTML/XML: {e}")
            raise ValueError(f"Invalid HTML/XML content: {e}")

    def _element_to_json(self, element: ET.Element) -> Dict[str, Any]:
        """
        Recursively converts an XML element to a UMG JSON object.
        """
        # 1. Determine Widget Class
        widget_class = self._map_tag_to_class(element.tag)
        
        # 2. Extract Name
        widget_name = element.get("Name", element.get("name", f"{element.tag}_{id(element)}"))
        
        # 3. Extract Properties and Slot Properties
        properties = {}
        slot_properties = {}
        
        for key, value in element.attrib.items():
            if key.lower() == "name":
                continue
            
            # Handle Slot Properties (e.g., Slot.Size, Slot.Position)
            if key.startswith("Slot."):
                slot_prop_name = key[5:] # Remove "Slot."
                slot_properties[slot_prop_name] = self._parse_value(value)
            else:
                properties[key] = self._parse_value(value)
        
        # 4. Construct JSON Object
        widget_json = {
            "widget_name": widget_name,
            "widget_class": widget_class,
            "properties": properties
        }
        
        if slot_properties:
            # In the current UMG JSON schema, Slot properties are usually nested under "Slot"
            # But 'apply_json_to_umg' might expect them in a specific way.
            # Let's look at UMGGet.py output: "Slot": { "Padding": ... } inside properties.
            widget_json["properties"]["Slot"] = slot_properties

        # 5. Process Children
        children = []
        for child in element:
            child_json = self._element_to_json(child)
            children.append(child_json)
        
        if children:
            widget_json["children"] = children
            
        return widget_json

    def _map_tag_to_class(self, tag: str) -> str:
        """
        Maps HTML tags to UMG Class paths.
        """
        # Common UMG Widgets
        mapping = {
            "Button": "/Script/UMG.Button",
            "TextBlock": "/Script/UMG.TextBlock",
            "Image": "/Script/UMG.Image",
            "CanvasPanel": "/Script/UMG.CanvasPanel",
            "VerticalBox": "/Script/UMG.VerticalBox",
            "HorizontalBox": "/Script/UMG.HorizontalBox",
            "Overlay": "/Script/UMG.Overlay",
            "Border": "/Script/UMG.Border",
            "EditableTextBox": "/Script/UMG.EditableTextBox",
            "ProgressBar": "/Script/UMG.ProgressBar",
            "Spacer": "/Script/UMG.Spacer",
            "SizeBox": "/Script/UMG.SizeBox",
            "ScrollBox": "/Script/UMG.ScrollBox",
            "GridPanel": "/Script/UMG.GridPanel",
            "UniformGridPanel": "/Script/UMG.UniformGridPanel",
            "WrapBox": "/Script/UMG.WrapBox",
            "WidgetSwitcher": "/Script/UMG.WidgetSwitcher",
            "SafeZone": "/Script/UMG.SafeZone",
            "ScaleBox": "/Script/UMG.ScaleBox",
            "InvalidationBox": "/Script/UMG.InvalidationBox",
            "RetainerBox": "/Script/UMG.RetainerBox",
            "CheckBox": "/Script/UMG.CheckBox",
            "Slider": "/Script/UMG.Slider",
            "ComboBoxString": "/Script/UMG.ComboBoxString",
            # HTML-like aliases
            "div": "/Script/UMG.CanvasPanel", # Default div to CanvasPanel? Or VerticalBox?
            "span": "/Script/UMG.TextBlock",
            "p": "/Script/UMG.TextBlock",
            "img": "/Script/UMG.Image",
            "button": "/Script/UMG.Button",
            "input": "/Script/UMG.EditableTextBox",
        }
        
        return mapping.get(tag, mapping.get(tag.lower(), f"/Script/UMG.{tag}")) # Fallback to trying the tag as a class name

    def _parse_value(self, value_str: str) -> Any:
        """
        Tries to parse string values into appropriate types (bool, float, json object).
        """
        # Boolean
        if value_str.lower() == "true": return True
        if value_str.lower() == "false": return False
        
        # Numbers
        try:
            if "." in value_str:
                return float(value_str)
            return int(value_str)
        except ValueError:
            pass
            
        # JSON Object/Array (e.g., "(X=1,Y=2)" or "{...}")
        # This is tricky. Unreal uses specific string formats for structs.
        # For now, return the string and let Unreal's FJsonObjectConverter handle it if possible,
        # OR try to parse simple JSON if it looks like JSON.
        if value_str.startswith("{") or value_str.startswith("["):
            try:
                return json.loads(value_str)
            except json.JSONDecodeError:
                pass
        
        return value_str
