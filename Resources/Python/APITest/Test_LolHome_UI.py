"""Test: LoL-style launcher home UI via UmgMcp socket bridge."""
import json
import socket
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from Bridge.UmgLayoutCompiler import UmgLayoutCompiler

HOST, PORT = "127.0.0.1", 55557
ASSET = "/Game/UI/Test/WBP_LolHomeTest"

# LoL client palette
C = {
    "AppBG": {"R": 0.04, "G": 0.05, "B": 0.08, "A": 1.0},
    "PanelBG": {"R": 0.06, "G": 0.07, "B": 0.10, "A": 0.92},
    "HeaderBG": {"R": 0.08, "G": 0.09, "B": 0.12, "A": 0.95},
    "Gold": {"R": 0.88, "G": 0.72, "B": 0.38, "A": 1.0},
    "Teal": {"R": 0.08, "G": 0.72, "B": 0.62, "A": 1.0},
    "PlayBtn": {"R": 0.05, "G": 0.45, "B": 0.55, "A": 1.0},
    "TextMain": {"R": 0.92, "G": 0.90, "B": 0.85, "A": 1.0},
    "TextDim": {"R": 0.55, "G": 0.55, "B": 0.55, "A": 1.0},
    "CTA": {"R": 0.12, "G": 0.78, "B": 0.45, "A": 1.0},
    "BannerBG": {"R": 0.10, "G": 0.18, "B": 0.14, "A": 1.0},
    "SidebarActive": {"R": 0.12, "G": 0.14, "B": 0.18, "A": 1.0},
}


def send(cmd: str, params: dict | None = None) -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(60)
    s.connect((HOST, PORT))
    payload = json.dumps({"command": cmd, "params": params or {}}).encode("utf-8") + b"\0"
    s.sendall(payload)
    s.shutdown(socket.SHUT_WR)
    data = b""
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        data += chunk
        if b"\0" in data:
            break
    s.close()
    return json.loads(data.split(b"\0")[0].decode("utf-8"))


def text_block(name: str, text: str, size: int = 12, color=None, bold: bool = False) -> dict:
    color = color or C["TextMain"]
    return {
        "type": "TextBlock",
        "name": name,
        "properties": {
            "Text": text,
            "Font": {"Size": size, "Typeface": "Bold" if bold else "Regular"},
            "ColorAndOpacity": {"SpecifiedColor": color},
        },
    }


def border_panel(name: str, bg, padding=(12, 8), child=None, flex: float = 0) -> dict:
    node = {
        "type": "Border",
        "name": name,
        "properties": {
            "BrushColor": bg,
            "Padding": {"Left": padding[0], "Right": padding[0], "Top": padding[1], "Bottom": padding[1]},
        },
    }
    if flex:
        node["flex"] = flex
    if child:
        node["children"] = [child]
    return node


def sidebar_item(name: str, label: str, active: bool = False) -> dict:
    bg = C["SidebarActive"] if active else {"R": 0, "G": 0, "B": 0, "A": 0}
    return border_panel(
        name,
        bg,
        padding=(16, 10),
        child=text_block(f"{name}_Txt", label, 11, C["TextMain"] if active else C["TextDim"], bold=active),
    )


def build_dsl() -> dict:
    top_bar = {
        "layout_type": "flex",
        "direction": "row",
        "name": "TopBar",
        "align": "center",
        "properties": {"BrushColor": C["HeaderBG"]},
        "children": [
            border_panel(
                "BtnPlay",
                C["PlayBtn"],
                padding=(28, 10),
                child=text_block("TxtPlay", "PLAY", 16, C["TextMain"], bold=True),
            ),
            border_panel("TabHome", C["SidebarActive"], padding=(14, 6), child=text_block("TxtHome", "主页", 11, C["TextMain"], bold=True)),
            border_panel("TabTFT", {"R": 0, "G": 0, "B": 0, "A": 0}, padding=(14, 6), child=text_block("TxtTFT", "云顶之弈", 11, C["TextDim"])),
            {"type": "Spacer", "name": "TopSpacer", "flex": 1},
            text_block("TxtCollection", "藏品", 10, C["TextDim"]),
            text_block("TxtLoot", "战利品/圣堂", 10, C["TextDim"]),
            text_block("TxtStore", "商城", 10, C["TextDim"]),
            border_panel("CurrencyRP", {"R": 0.1, "G": 0.1, "B": 0.12, "A": 1}, padding=(8, 4), child=text_block("TxtRP", "50", 10, C["Gold"])),
            border_panel("CurrencyBE", {"R": 0.1, "G": 0.1, "B": 0.12, "A": 1}, padding=(8, 4), child=text_block("TxtBE", "21.8万", 10, C["Teal"])),
        ],
    }

    sidebar = {
        "layout_type": "flex",
        "direction": "column",
        "name": "Sidebar",
        "gap": 2,
        "properties": {"BrushColor": C["PanelBG"]},
        "children": [
            sidebar_item("NavHome", "首页", active=True),
            sidebar_item("NavSeason", "海斗出招季"),
            sidebar_item("NavSummon", "召唤中心"),
            sidebar_item("NavEvent", "活动中心"),
            sidebar_item("NavVex", "7周年小小薇古丝"),
            sidebar_item("NavChef", "大厨奥义宝典"),
            sidebar_item("NavDragon", "银龙征程"),
            {"type": "Spacer", "name": "SidebarSpacer", "flex": 1},
            text_block("TxtGuide", "攻略  |  更多", 9, C["TextDim"]),
            text_block("TxtVersion", "版本公告", 9, C["TextDim"]),
        ],
    }

    banner = {
        "layout_type": "absolute",
        "name": "BannerArea",
        "properties": {"BrushColor": C["BannerBG"]},
        "children": [
            {
                "type": "VerticalBox",
                "name": "BannerContent",
                "position": "center",
                "children": [
                    text_block("TxtPromo1", "首次6元100%解锁1款未拥有皮肤", 12, C["TextMain"]),
                    text_block("TxtTitle", "华彩秘宝 · 召唤", 36, C["Gold"], bold=True),
                    text_block("TxtPromo2", "每10次开启额外解锁一款随机臻彩", 12, C["TextDim"]),
                    border_panel(
                        "BtnGo",
                        C["CTA"],
                        padding=(48, 12),
                        child=text_block("TxtGo", "立即前往", 14, C["TextMain"], bold=True),
                    ),
                ],
            },
            {
                "type": "Border",
                "name": "ArrowLeft",
                "position": "left-center",
                "properties": {
                    "BrushColor": {"R": 0, "G": 0, "B": 0, "A": 0.5},
                    "Padding": {"Left": 8, "Right": 8, "Top": 16, "Bottom": 16},
                },
                "children": [text_block("TxtArrowL", "<", 18, C["TextMain"])],
            },
            {
                "type": "Border",
                "name": "ArrowRight",
                "position": "right-center",
                "properties": {
                    "BrushColor": {"R": 0, "G": 0, "B": 0, "A": 0.5},
                    "Padding": {"Left": 8, "Right": 8, "Top": 16, "Bottom": 16},
                },
                "children": [text_block("TxtArrowR", ">", 18, C["TextMain"])],
            },
            {
                "type": "Border",
                "name": "FloatTab",
                "position": "right-center",
                "properties": {
                    "BrushColor": {"R": 0.08, "G": 0.10, "B": 0.14, "A": 0.9},
                    "Padding": {"Left": 6, "Right": 6, "Top": 20, "Bottom": 20},
                },
                "children": [text_block("TxtFloat", "幸运之门\n召唤", 9, C["Gold"])],
            },
        ],
    }

    main_row = {
        "layout_type": "flex",
        "direction": "row",
        "name": "MainRow",
        "align": "stretch",
        "children": [
            {**sidebar, "flex": 0, "properties": {**sidebar.get("properties", {}), "Slot": {"Size": {"SizeRule": "Auto"}}}},
            {**banner, "flex": 1},
        ],
    }

    root = {
        "layout_type": "flex",
        "direction": "column",
        "name": "Root",
        "properties": {"BrushColor": C["AppBG"]},
        "children": [
            top_bar,
            {**main_row, "flex": 1},
        ],
    }
    return root


def main() -> None:
    print("=== UmgMcp LoL Home UI Test ===")

    r = send("set_target_umg_asset", {"asset_path": ASSET})
    print(f"[1] set_target: {r.get('status')} action={r.get('action', 'existing')}")

    dsl = build_dsl()
    compiled, lint = UmgLayoutCompiler.compile(dsl)
    print(f"[2] compile: ok={lint.ok}, warnings={len(lint.warnings)}, errors={len(lint.errors)}")
    if not lint.ok:
        print(json.dumps(lint.to_dict(), ensure_ascii=False, indent=2))
        sys.exit(1)

    r = send(
        "apply_json_to_umg",
        {
            "asset_path": ASSET,
            "json_data": json.dumps(compiled, ensure_ascii=False),
            "widget_name": "Root",
        },
    )
    print(f"[3] apply: status={r.get('status')} success={r.get('success')}")
    if r.get("status") == "error" or r.get("success") is False:
        print(json.dumps(r, ensure_ascii=False, indent=2)[:3000])
        sys.exit(1)

    r = send("save_asset", {"asset_path": ASSET})
    print(f"[4] save: {r.get('status')}")

    r = send("get_widget_tree", {})
    tree = r.get("widget_tree", "")
    lines = [ln for ln in tree.split("\n") if ln.strip()]
    print(f"[5] widget_tree ({len(lines)} lines):")
    for ln in lines[:30]:
        print(f"    {ln}")
    if len(lines) > 30:
        print(f"    ... +{len(lines) - 30} more")

    r = send("lint_umg_asset", {"asset_path": ASSET})
    summary = r.get("summary") or r.get("lint_summary") or {}
    print(f"[6] lint: {summary if summary else r.get('status')}")

    print("\nDone. Open WBP_LolHomeTest in UE Designer to preview.")


if __name__ == "__main__":
    main()
