import json
from pathlib import Path

DEFAULT = {
    "path": r"D:\asxiuxiu_obsidian_repo",
    "vault_name": "asxiuxiu_obsidian_repo"
}


def main():
    config_path = Path(".vault-context.json")
    if config_path.exists():
        try:
            data = json.loads(config_path.read_text(encoding="utf-8"))
            result = {
                "path": data.get("path", DEFAULT["path"]),
                "vault_name": data.get("vault_name", DEFAULT["vault_name"])
            }
            print(json.dumps(result, ensure_ascii=False))
            return
        except Exception:
            pass
    print(json.dumps(DEFAULT, ensure_ascii=False))


if __name__ == "__main__":
    main()
