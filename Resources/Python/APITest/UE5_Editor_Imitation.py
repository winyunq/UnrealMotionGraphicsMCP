import asyncio
import json
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# ==========================================
# 🎨 UE5 "DARK MODE" PALETTE (PIXEL PERFECT)
# ==========================================
C = {
    "AppBG":        {"R": 0.06, "G": 0.06, "B": 0.06, "A": 1.0}, # Main Background
    "PanelBG":      {"R": 0.11, "G": 0.11, "B": 0.11, "A": 1.0}, # Panels
    "HeaderBG":     {"R": 0.16, "G": 0.16, "B": 0.16, "A": 1.0}, # Headers
    "TabActive":    {"R": 0.11, "G": 0.11, "B": 0.11, "A": 1.0}, # Active Tab
    "TabInactive":  {"R": 0.08, "G": 0.08, "B": 0.08, "A": 1.0}, # Inactive Tab
    "InputBG":      {"R": 0.05, "G": 0.05, "B": 0.05, "A": 1.0}, # Input Fields
    "Border":       {"R": 0.02, "G": 0.02, "B": 0.02, "A": 1.0}, # Borders
    "Selection":    {"R": 0.0,  "G": 0.36, "B": 0.62, "A": 1.0}, # UE Blue Selection
    "TextMain":     {"R": 0.9,  "G": 0.9,  "B": 0.9,  "A": 1.0},
    "TextDim":      {"R": 0.5,  "G": 0.5,  "B": 0.5,  "A": 1.0},
    "IconGray":     {"R": 0.7,  "G": 0.7,  "B": 0.7,  "A": 1.0},
    "GreenAccent":  {"R": 0.25, "G": 0.65, "B": 0.25, "A": 1.0}, # Add Button
    "Folder":       {"R": 0.86, "G": 0.71, "B": 0.48, "A": 1.0}, # Folder Color
    "Blueprint":    {"R": 0.0,  "G": 0.36, "B": 0.62, "A": 1.0}, # Blueprint Blue
    "RedAxis":      {"R": 0.85, "G": 0.1,  "B": 0.1,  "A": 1.0},
    "GreenAxis":    {"R": 0.1,  "G": 0.85, "B": 0.1,  "A": 1.0},
    "BlueAxis":     {"R": 0.1,  "G": 0.1,  "B": 0.95, "A": 1.0},
    "ZenGreen":     {"R": 0.2,  "G": 0.8,  "B": 0.2,  "A": 1.0}, # Zen Server Status
}

async def run_demo():
    server_params = StdioServerParameters(
        command="python",
        args=["../UmgMcpServer.py"],
        env=None
    )

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            print("\n" + "="*80)
            print("🌌 GEMINI SHOCKING DEMO - UE5 EDITOR RECREATION")
            print("="*80)
            
            # --- HELPER FUNCTIONS ---
            async def create(parent, type, name):
                await session.call_tool("create_widget", arguments={
                    "parent_name": parent, "widget_type": type, "new_widget_name": name
                })
                return name

            async def props(name, p_dict):
                await session.call_tool("set_widget_properties", arguments={
                    "widget_name": name, "properties": json.dumps(p_dict)
                })

            async def create_text(parent, name, text, size=9, color=C["TextMain"], bold=False, italic=False):
                w = await create(parent, "TextBlock", name)
                typeface = "Bold" if bold else ("Italic" if italic else "Regular")
                await props(w, {
                    "Text": text, "Font": {"Size": size, "Typeface": typeface},
                    "ColorAndOpacity": {"SpecifiedColor": color},
                    "Slot": {"VerticalAlignment": "Center"}
                })
                return w

            async def create_icon_btn(parent, name, icon, text="", bg=None, color=C["TextMain"], padding=6):
                btn = await create(parent, "Border", name)
                await props(btn, {
                    "BrushColor": bg if bg else {"R":0,"G":0,"B":0,"A":0},
                    "Padding": {"Left": padding, "Right": padding, "Top": 2, "Bottom": 2},
                    "Slot": {"Padding": {"Right": 2}, "VerticalAlignment": "Center"}
                })
                box = await create(btn, "HorizontalBox", f"{name}_Box")
                if icon:
                    ico = await create_text(box, f"{name}_Icon", icon, 10, color)
                    await props(ico, {"Slot": {"Padding": {"Right": 4 if text else 0}}})
                if text:
                    txt = await create_text(box, f"{name}_Text", text, 9, color, bold=True)
                return btn

            async def create_tab(parent, name, text, active=False):
                bg = C["TabActive"] if active else C["TabInactive"]
                fg = C["TextMain"] if active else C["TextDim"]
                tab = await create(parent, "Border", name)
                await props(tab, {
                    "BrushColor": bg,
                    "Padding": {"Left": 10, "Right": 10, "Top": 4, "Bottom": 4},
                    "Slot": {"Padding": {"Right": 1}}
                })
                box = await create(tab, "HorizontalBox", f"{name}_Box")
                await create_text(box, f"{name}_Txt", text, 9, fg, bold=active)
                if active:
                    x = await create_text(box, f"{name}_X", " ✕", 8, C["TextDim"])
                    await props(x, {"Slot": {"Padding": {"Left": 8}}})

            async def create_tree_row(parent, name, label, type_text, indent=0, selected=False, is_folder=False):
                row_bg = await create(parent, "Border", name)
                bg_color = C["Selection"] if selected else {"R":0,"G":0,"B":0,"A":0}
                await props(row_bg, {"BrushColor": bg_color, "Padding": {"Top":1, "Bottom":1}})
                
                hb = await create(row_bg, "HorizontalBox", f"{name}_HB")
                
                # Label Part
                lbl_box = await create(hb, "HorizontalBox", f"{name}_LblBox")
                await props(lbl_box, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.6}}})
                
                if indent > 0:
                    sp = await create(lbl_box, "Spacer", f"{name}_Ind")
                    await props(sp, {"Size": {"X": indent * 16, "Y": 1}})
                
                arrow = "▼" if is_folder else " "
                await create_text(lbl_box, f"{name}_Arr", arrow, 8, C["TextDim"])
                
                icon = "📁" if is_folder else "🧊"
                icon_col = C["Folder"] if is_folder else (C["TextMain"] if selected else {"R":0.4,"G":0.6,"B":1,"A":1})
                ico = await create_text(lbl_box, f"{name}_Ico", icon + " ", 10, icon_col)
                await props(ico, {"Slot": {"Padding": {"Left": 4, "Right": 4}}})
                
                await create_text(lbl_box, f"{name}_Txt", label, 9, C["TextMain"])
                
                # Controls (Eye)
                eye = await create(hb, "TextBlock", f"{name}_Eye")
                await props(eye, {"Text": "👁", "Font": {"Size": 9}, "ColorAndOpacity": {"SpecifiedColor": C["TextDim"]}, "Slot": {"Padding": {"Right": 10}}})
                
                # Type Part
                await create_text(hb, f"{name}_Type", type_text, 8, C["TextDim"])

            async def create_prop_row(parent, name, label, value_widget_type="Text", value_data=None):
                row = await create(parent, "Border", name)
                await props(row, {"BrushColor": {"R":0,"G":0,"B":0,"A":0}, "Padding": {"Top": 2, "Bottom": 2}})
                hb = await create(row, "HorizontalBox", f"{name}_HB")
                
                # Label
                lbl_box = await create(hb, "HorizontalBox", f"{name}_Lbl")
                await props(lbl_box, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.35}}})
                await create_text(lbl_box, f"{name}_Arrow", "▼ ", 8, C["TextDim"])
                await create_text(lbl_box, f"{name}_Txt", label, 9, C["TextMain"])
                
                # Value Area
                val_box = await create(hb, "Border", f"{name}_ValBox")
                await props(val_box, {"BrushColor": {"R":0,"G":0,"B":0,"A":0}, "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.65}}})
                
                if value_widget_type == "Transform":
                    # X Y Z Inputs
                    t_box = await create(val_box, "VerticalBox", f"{name}_TBox")
                    for axis, val, col in [("Location", value_data[0], C["RedAxis"]), ("Rotation", value_data[1], C["GreenAxis"]), ("Scale", value_data[2], C["BlueAxis"])]:
                        ax_row = await create(t_box, "HorizontalBox", f"{name}_{axis}")
                        await props(ax_row, {"Slot": {"Padding": {"Bottom": 2}}})
                        
                        # Label
                        l_row = await create(ax_row, "HorizontalBox", f"{name}_{axis}_L")
                        await props(l_row, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.3}}})
                        await create_text(l_row, f"{name}_{axis}_Txt", axis, 9, C["TextMain"])
                        
                        # Inputs
                        i_row = await create(ax_row, "HorizontalBox", f"{name}_{axis}_I")
                        await props(i_row, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.7}}})
                        
                        for ax_char, ax_val, ax_col in [("X", val[0], C["RedAxis"]), ("Y", val[1], C["GreenAxis"]), ("Z", val[2], C["BlueAxis"])]:
                            # Axis Label
                            ab = await create(i_row, "Border", f"{name}_{axis}_{ax_char}_B")
                            await props(ab, {"BrushColor": ax_col, "Padding": {"Left":4,"Right":4}})
                            await create_text(ab, f"{name}_{axis}_{ax_char}_T", ax_char, 8, {"R":0,"G":0,"B":0,"A":1}, bold=True)
                            
                            # Value
                            vb = await create(i_row, "Border", f"{name}_{axis}_{ax_char}_V")
                            await props(vb, {"BrushColor": C["InputBG"], "Padding": {"Left":5,"Right":5}, "Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}, "Padding": {"Right": 4}}})
                            await create_text(vb, f"{name}_{axis}_{ax_char}_VT", ax_val, 9, C["TextMain"])

                elif value_widget_type == "Info":
                    # Read-only text
                    await create_text(val_box, f"{name}_Info", value_data, 9, C["TextDim"])
                
                elif value_widget_type == "Dropdown":
                    db = await create(val_box, "Border", f"{name}_Drop")
                    await props(db, {"BrushColor": C["InputBG"], "Padding": {"Left":5,"Right":5,"Top":2,"Bottom":2}})
                    await create_text(db, f"{name}_DropTxt", value_data, 9, C["TextMain"])

            # ==========================================
            # 🚀 BUILD SEQUENCE
            # ==========================================
            
            # 0. Root
            print("🔨 Initializing Root...")
            await session.call_tool("apply_layout", arguments={"layout_content": json.dumps({
                "widget_name": "UE5_Root", "widget_class": "/Script/UMG.VerticalBox",
                "properties": {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}}
            })})

            # 1. Menu Bar
            print("🔨 Menu Bar...")
            menu_bg = await create("UE5_Root", "Border", "MenuBar")
            await props(menu_bg, {"BrushColor": C["AppBG"], "Padding": {"Left": 5, "Right": 5, "Top": 1, "Bottom": 1}, "Slot": {"Size": {"SizeRule": "Auto"}}})
            menu_hb = await create(menu_bg, "HorizontalBox", "MenuHB")
            
            # Logo
            await create_icon_btn(menu_hb, "Logo", "ⓤ", "", None, C["TextDim"])
            
            # Menus
            for m in ["File", "Edit", "Window", "Tools", "Build", "Select", "Actor", "Help"]:
                await create_icon_btn(menu_hb, f"Menu_{m}", "", m, None, C["TextMain"], padding=8)
            
            # Right side window controls
            await create(menu_hb, "Spacer", "MenuSpacer")
            await props("MenuSpacer", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            await create_text(menu_hb, "WinTitle", "UnrealMotionGraphicsMCP  ", 9, C["TextDim"])
            # Window Controls with Safe Names
            await create_icon_btn(menu_hb, "Win_Minimize", "", "─", None, C["TextDim"], padding=10)
            await create_icon_btn(menu_hb, "Win_Maximize", "", "☐", None, C["TextDim"], padding=10)
            await create_icon_btn(menu_hb, "Win_Close", "", "✕", None, C["TextDim"], padding=10)

            # 2. Toolbar
            print("🔨 Toolbar...")
            tool_bg = await create("UE5_Root", "Border", "Toolbar")
            await props(tool_bg, {"BrushColor": C["PanelBG"], "Padding": {"Left": 5, "Right": 5, "Top": 3, "Bottom": 3}, "Slot": {"Size": {"SizeRule": "Auto"}}})
            tool_hb = await create(tool_bg, "HorizontalBox", "ToolHB")
            
            await create_icon_btn(tool_hb, "Save", "💾", "Save")
            await create_icon_btn(tool_hb, "Mode", "⛶", "Selection Mode")
            await create(tool_hb, "Spacer", "Sep1")
            await props("Sep1", {"Size": {"X": 10, "Y": 1}})
            await create_icon_btn(tool_hb, "Create", "✚", "Create")
            await create_icon_btn(tool_hb, "BP", "💠", "Blueprints")
            await create(tool_hb, "Spacer", "Sep2")
            await props("Sep2", {"Size": {"X": 10, "Y": 1}})
            await create_icon_btn(tool_hb, "Play", "▶", "Play", C["GreenAccent"], padding=12)
            await create_icon_btn(tool_hb, "Platforms", "☁", "Platforms")
            
            await create(tool_hb, "Spacer", "ToolSpacer")
            await props("ToolSpacer", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            await create_icon_btn(tool_hb, "Settings", "⚙", "Settings")

            # 3. Main Workspace
            print("🔨 Workspace...")
            workspace = await create("UE5_Root", "HorizontalBox", "Workspace")
            await props(workspace, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            
            # --- LEFT COLUMN (75%) ---
            left_col = await create(workspace, "VerticalBox", "LeftCol")
            await props(left_col, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.75}}})
            
            # 3.1 VIEWPORT
            vp_overlay = await create(left_col, "Overlay", "VPOverlay")
            await props(vp_overlay, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.7}}})
            
            # BG (The Game)
            vp_bg = await create(vp_overlay, "Border", "VPBG")
            await props(vp_bg, {"BrushColor": {"R":0.1,"G":0.1,"B":0.12,"A":1}})
            
            # Top Bar
            vp_top = await create(vp_overlay, "Border", "VPTop")
            await props(vp_top, {"BrushColor": {"R":0,"G":0,"B":0,"A":0.4}, "Padding": {"Left":5,"Right":5,"Top":2,"Bottom":2}, "Slot": {"VerticalAlignment": "Top"}})
            vp_top_hb = await create(vp_top, "HorizontalBox", "VPTopHB")
            
            await create_icon_btn(vp_top_hb, "VPOpt", "▼", "")
            await create_icon_btn(vp_top_hb, "VPPersp", "Perspective", "")
            await create_icon_btn(vp_top_hb, "VPLit", "Lit", "")
            await create_icon_btn(vp_top_hb, "VPShow", "Show", "")
            
            await create(vp_top_hb, "Spacer", "VPSp")
            await props("VPSp", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            
            for t in ["Move", "Rot", "Scale"]:
                 await create_icon_btn(vp_top_hb, f"VP_{t}", "⌗", "")
            await create_text(vp_top_hb, "VPCam", "  📷 4  ", 9, C["TextMain"])
            
            # Bottom Left Axis
            vp_axis = await create(vp_overlay, "Border", "VPAxis")
            await props(vp_axis, {"BrushColor": {"R":0,"G":0,"B":0,"A":0}, "Padding": {"Left": 10, "Bottom": 10}, "Slot": {"HorizontalAlignment": "Left", "VerticalAlignment": "Bottom"}})
            await create_text(vp_axis, "VPAxisTxt", "↙ Z\n→ Y\n↘ X", 10, C["TextDim"], bold=True)

            # 3.2 CONTENT BROWSER
            cb_frame = await create(left_col, "Border", "CBFrame")
            await props(cb_frame, {"BrushColor": C["PanelBG"], "Padding": {"Top": 2}, "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.3}}})
            cb_box = await create(cb_frame, "VerticalBox", "CBBox")
            
            # Tabs
            cb_tabs = await create(cb_box, "HorizontalBox", "CBTabs")
            await create_tab(cb_tabs, "Tab_CB", "Content Browser 1", True)
            await create_tab(cb_tabs, "Tab_Log", "Output Log", False)
            await create(cb_tabs, "Spacer", "TabSp")
            await props("TabSp", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            
            # Toolbar
            cb_tool = await create(cb_box, "HorizontalBox", "CBTool")
            await props(cb_tool, {"Slot": {"Padding": {"Top": 4, "Bottom": 4, "Left": 4}}})
            await create_icon_btn(cb_tool, "CBAdd", "✚", "Add", C["GreenAccent"])
            await create_icon_btn(cb_tool, "CBImport", "⬇", "Import")
            await create_icon_btn(cb_tool, "CBSave", "💾", "Save All")
            
            # Path
            cb_path = await create(cb_box, "Border", "CBPath")
            await props(cb_path, {"BrushColor": C["InputBG"], "Padding": {"Left": 8, "Top": 3, "Bottom": 3}})
            await create_text(cb_path, "CBPathTxt", "All > Content > Developers > UmgMcp > Resources", 9, C["TextDim"])
            
            # Split
            cb_split = await create(cb_box, "HorizontalBox", "CBSplit")
            await props(cb_split, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            
            # Tree
            cb_tree = await create(cb_split, "Border", "CBTree")
            await props(cb_tree, {"BrushColor": C["AppBG"], "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.2}}})
            cb_tree_box = await create(cb_tree, "VerticalBox", "CBTreeBox")
            for f in ["All", "Content", "Developers", "Engine", "Plugins"]:
                await create_tree_row(cb_tree_box, f"Tree_{f}", f, "", 0, f=="Developers", True)
            
            # Grid
            cb_grid = await create(cb_split, "Border", "CBGrid")
            await props(cb_grid, {"BrushColor": C["PanelBG"], "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.8}, "Padding": {"Left": 2}}})
            
            # Asset Item
            asset_box = await create(cb_grid, "VerticalBox", "AssetBox")
            await props(asset_box, {"Slot": {"Padding": {"Left": 10, "Top": 10}, "HorizontalAlignment": "Left", "VerticalAlignment": "Top"}})
            
            thumb = await create(asset_box, "Border", "Thumb")
            await props(thumb, {"BrushColor": {"R":0.2,"G":0.2,"B":0.2,"A":1}, "Slot": {"Size": {"SizeRule": "Auto"}}})
            await create(thumb, "Spacer", "ThumbSp")
            await props("ThumbSp", {"Size": {"X": 80, "Y": 80}})
            
            stripe = await create(asset_box, "Border", "Stripe")
            await props(stripe, {"BrushColor": C["Blueprint"], "Slot": {"Size": {"SizeRule": "Auto"}}})
            await create(stripe, "Spacer", "StripeSp")
            await props("StripeSp", {"Size": {"X": 80, "Y": 4}})
            
            await create_text(asset_box, "AssetNm", "NewWidgetBP", 9, C["TextMain"])
            
            # Bottom Bar
            cb_bot = await create(cb_box, "Border", "CBBot")
            await props(cb_bot, {"BrushColor": C["HeaderBG"], "Padding": {"Left": 5, "Top": 2, "Bottom": 2}})
            await create_text(cb_bot, "CBBotTxt", "1 item (1 selected)", 8, C["TextMain"])

            # --- RIGHT COLUMN (25%) ---
            right_col = await create(workspace, "VerticalBox", "RightCol")
            await props(right_col, {"Slot": {"Size": {"SizeRule": "Fill", "Value": 0.25}, "Padding": {"Left": 2}}})
            
            # 3.3 OUTLINER
            out_frame = await create(right_col, "Border", "OutFrame")
            await props(out_frame, {"BrushColor": C["PanelBG"], "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.4}}})
            out_box = await create(out_frame, "VerticalBox", "OutBox")
            
            # Tabs
            out_tabs = await create(out_box, "HorizontalBox", "OutTabs")
            await create_tab(out_tabs, "Tab_Out", "Outliner", True)
            await create_tab(out_tabs, "Tab_Lay", "Layers", False)
            
            # Search
            out_search = await create(out_box, "Border", "OutSearch")
            await props(out_search, {"BrushColor": C["InputBG"], "Padding": {"Left": 5, "Top": 3, "Bottom": 3}, "Slot": {"Padding": {"Left": 2, "Right": 2, "Top": 2, "Bottom": 2}}})
            await create_text(out_search, "OutSearchTxt", "🔍 Search...", 8, C["TextDim"], italic=True)
            
            # Header
            out_head = await create(out_box, "Border", "OutHead")
            await props(out_head, {"BrushColor": C["HeaderBG"], "Padding": {"Left": 5, "Top": 2, "Bottom": 2}})
            await create_text(out_head, "OutHeadTxt", "Item Label                    Type", 8, C["TextDim"], bold=True)
            
            # List
            await create_tree_row(out_box, "Row_World", "Untitled (Editor)", "World", 0, False, True)
            await create_tree_row(out_box, "Row_Light", "Lighting", "Folder", 1, False, True)
            await create_tree_row(out_box, "Row_Dir", "DirectionalLight", "DirectionalLight", 2, False, False)
            await create_tree_row(out_box, "Row_Sky", "SkyAtmosphere", "SkyAtmosphere", 2, False, False)
            await create_tree_row(out_box, "Row_Land", "Landscape", "Landscape", 1, True, False) # SELECTED!
            await create_tree_row(out_box, "Row_Start", "PlayerStart", "PlayerStart", 1, False, False)

            # 3.4 DETAILS
            det_frame = await create(right_col, "Border", "DetFrame")
            await props(det_frame, {"BrushColor": C["PanelBG"], "Slot": {"Size": {"SizeRule": "Fill", "Value": 0.6}, "Padding": {"Top": 2}}})
            det_box = await create(det_frame, "VerticalBox", "DetBox")
            
            # Tabs
            det_tabs = await create(det_box, "HorizontalBox", "DetTabs")
            await create_tab(det_tabs, "Tab_Det", "Details", True)
            
            # Object Name
            obj_row = await create(det_box, "HorizontalBox", "ObjRow")
            await props(obj_row, {"Slot": {"Padding": {"Left": 10, "Top": 5, "Bottom": 5}}})
            await create_text(obj_row, "ObjIcon", "⛰ ", 12, C["TextMain"])
            
            obj_name_box = await create(obj_row, "VerticalBox", "ObjNameBox")
            await create_text(obj_name_box, "ObjName", "Landscape", 10, C["TextMain"], bold=True)
            await create_text(obj_name_box, "ObjClass", "Landscape (Instance)", 8, C["Selection"])
            
            await create(obj_row, "Spacer", "ObjSp")
            await props("ObjSp", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            await create_icon_btn(obj_row, "AddComp", "✚", "Add", C["GreenAccent"])
            
            # Search
            det_search = await create(det_box, "Border", "DetSearch")
            await props(det_search, {"BrushColor": C["InputBG"], "Padding": {"Left": 5, "Top": 3, "Bottom": 3}, "Slot": {"Padding": {"Left": 2, "Right": 2, "Bottom": 5}}})
            await create_text(det_search, "DetSearchTxt", "🔍 Search Details", 8, C["TextDim"], italic=True)
            
            # Properties
            # Transform
            await create_prop_row(det_box, "Prop_Trans", "Transform", "Transform", [
                ("-100800.0", "-100800.0", "100.0"),
                ("0.0", "0.0", "0.0"),
                ("100.0", "100.0", "100.0")
            ])
            
            # Information
            await create_prop_row(det_box, "Prop_Info", "Information", "Info", "Landscape Proxy Count: 64")
            await create_prop_row(det_box, "Prop_Res", "Resolution", "Info", "Overall Resolution: 2017x2017")
            
            # Virtual Texture
            await create_prop_row(det_box, "Prop_VT", "Virtual Texture", "Dropdown", "Draw in Main Pass")
            
            # Rendering
            await create_prop_row(det_box, "Prop_Rend", "Rendering", "Dropdown", "Actor Hidden In Game: False")
            
            # Landscape
            await create_prop_row(det_box, "Prop_Land", "Landscape", "Dropdown", "Enable Edit Layers: True")

            # 4. Status Bar
            print("🔨 Status Bar...")
            stat_bg = await create("UE5_Root", "Border", "StatBG")
            await props(stat_bg, {"BrushColor": C["PanelBG"], "Padding": {"Left": 0, "Right": 5, "Top": 0, "Bottom": 0}, "Slot": {"Size": {"SizeRule": "Auto"}}})
            stat_hb = await create(stat_bg, "HorizontalBox", "StatHB")
            
            await create_icon_btn(stat_hb, "Drawer", "🗄", "Content Drawer", C["HeaderBG"])
            await create_icon_btn(stat_hb, "Log", "Output Log", "")
            
            # Cmd Input
            cmd_bg = await create(stat_hb, "Border", "CmdBG")
            await props(cmd_bg, {"BrushColor": C["InputBG"], "Padding": {"Left": 5, "Right": 5}, "Slot": {"Padding": {"Left": 10, "Top": 2, "Bottom": 2}}})
            await create_text(cmd_bg, "CmdTxt", "Enter Console Command", 9, C["TextDim"], italic=True)
            
            await create(stat_hb, "Spacer", "StatSp")
            await props("StatSp", {"Slot": {"Size": {"SizeRule": "Fill", "Value": 1.0}}})
            
            # Right Status
            await create_icon_btn(stat_hb, "Source", "🚫", "Source Control Off")
            
            # Zen Server
            zen_box = await create(stat_hb, "HorizontalBox", "ZenBox")
            await props(zen_box, {"Slot": {"Padding": {"Left": 15, "Right": 5}, "VerticalAlignment": "Center"}})
            zen_dot = await create_text(zen_box, "ZenDot", "●", 8, C["ZenGreen"])
            await create_text(zen_box, "ZenTxt", " Zen Server", 9, C["TextDim"])
            
            await create_icon_btn(stat_hb, "Rev", "Revision Control", "")

            # SAVE
            print("💾 Saving Asset...")
            await session.call_tool("save_asset", arguments={})
            
            print("\n" + "="*80)
            print("✅ GEMINI SHOCKING RECREATION COMPLETE!")
            print("="*80)

if __name__ == "__main__":
    asyncio.run(run_demo())
