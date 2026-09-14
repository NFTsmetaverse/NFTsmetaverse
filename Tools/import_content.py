"""
Builds the fourteen content tables as DataTable assets.

Run it headless, from the project folder:

    UnrealEditor-Cmd.exe ChainOfWitnesses.uproject -run=pythonscript -script="Tools/import_content.py"

or from the open editor: Tools -> Execute Python Script.

**Why this exists.** Doing it by hand is fourteen drag-and-drops, each ending in a
dropdown listing every struct in the project. Pick the wrong one and the table
imports zero rows and says nothing about why -- and the failure does not surface
until the bootstrap logs an empty registry at runtime, a long way from the mistake.

**Why it builds the assets instead of importing them.** Driving CSVImportFactory
through AssetImportTask crashes the editor outright on 5.8 (assertion in
AssetTools, an invalid shared pointer under the factory's automated settings).
Creating the asset with DataTableFactory and filling it with
FillDataTableFromJSONString reaches the same end state through supported calls,
and reports its own failures instead of taking the process down.

The mapping below is the one docs/SETUP.md publishes and Tools/datatable_lint.py
derives from the headers.
"""

import os
import unreal

# asset name (and JSON basename) -> row struct, without the F prefix
MAPPING = [
    ("DT_CampaignEvents",            "CampaignEventRow"),
    ("DT_TestimonyFragments",        "TestimonyFragmentDefinition"),
    ("DT_Locations",                 "CampaignLocationRow"),
    ("DT_TravelRoutes",              "TravelRouteRow"),
    ("DT_EncounterTypes",            "EncounterTypeRow"),
    ("DT_Factions",                  "FactionRow"),
    ("DT_DeedTypes",                 "DeedTypeRow"),
    ("DT_Dialogue_JerusalemHandoff", "DialogueNodeRow"),
    ("DT_Dialogue_RomeAD65",         "DialogueNodeRow"),
    ("DT_DebateOpponents",           "DebateOpponentRow"),
    ("DT_DebateObjections",          "DebateObjectionRow"),
    ("DT_Eras",                      "EraDefinitionRow"),
    ("DT_WitnessMissions",           "WitnessMissionRow"),
    ("DT_CombatEncounters",          "CombatEncounterRow"),
]

DEST = "/Game/Data"
MODULE = "/Script/ChainOfWitnesses."


def row_count(table):
    try:
        return len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))
    except Exception:
        return 0


def build(asset_name, struct_name, source_dir):
    json_path = os.path.join(source_dir, asset_name + ".json")
    if not os.path.exists(json_path):
        unreal.log_error("[import] no such file: %s" % json_path)
        return 0

    struct = unreal.load_object(None, MODULE + struct_name)
    if struct is None:
        unreal.log_error("[import] row struct %s not found -- has the module compiled?"
                         % struct_name)
        return 0

    with open(json_path, "r", encoding="utf-8") as handle:
        json_text = handle.read()

    asset_path = "%s/%s" % (DEST, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct)

    # Confirm the struct actually took. A DataTableFactory with no row struct
    # refuses to create anything, and it refuses silently.
    applied = factory.get_editor_property("struct")
    if applied is None:
        unreal.log_error("[import] %s: factory would not accept row struct %s"
                         % (asset_name, struct_name))
        return 0

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    table = tools.create_asset(asset_name, DEST, unreal.DataTable, factory)
    if table is None:
        unreal.log_error("[import] could not create asset %s" % asset_path)
        return 0

    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json_text)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    return row_count(table)


def main():
    source_dir = os.path.join(unreal.Paths.project_dir(), "Content", "Data")
    unreal.log("[import] reading from %s" % source_dir)

    # Content/Data holds only JSON, which UE does not index, so /Game/Data may not
    # exist as a package path at all -- and create_asset into a directory that is
    # not there fails without saying so.
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.log("[import] creating %s" % DEST)
        unreal.EditorAssetLibrary.make_directory(DEST)

    results = []
    for asset_name, struct_name in MAPPING:
        try:
            rows = build(asset_name, struct_name, source_dir)
        except Exception as error:
            unreal.log_error("[import] %s raised: %s" % (asset_name, error))
            rows = 0
        results.append((asset_name, struct_name, rows))

    unreal.log("[import] ---- results ----")
    total = 0
    failed = []
    for asset_name, struct_name, rows in results:
        total += rows
        # A table with no rows is a wrong row struct, not a success.
        if rows == 0:
            failed.append(asset_name)
            unreal.log_error("[import] %-32s FAILED (0 rows, struct %s)"
                             % (asset_name, struct_name))
        else:
            unreal.log("[import] %-32s %4d rows" % (asset_name, rows))

    unreal.log("[import] %d of %d tables, %d rows total"
               % (len(MAPPING) - len(failed), len(MAPPING), total))
    if failed:
        unreal.log_error("[import] FAILED: %s" % ", ".join(failed))
    else:
        unreal.log("[import] ALL TABLES IMPORTED")


main()
