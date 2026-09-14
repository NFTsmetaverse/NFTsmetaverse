"""
Imports the fourteen content tables as DataTable assets.

Run it headless:

    UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript -script="Tools/import_content.py"

or from the open editor: Tools -> Execute Python Script.

**Why this exists.** Doing it by hand is fourteen drag-and-drops, each ending in a
dropdown listing every struct in the project. Pick the wrong one and the table
imports zero rows and says nothing about why -- and the failure does not surface
until the bootstrap logs an empty registry at runtime, a long way from the mistake.
The mapping below is the one docs/SETUP.md publishes and Tools/datatable_lint.py
derives from the headers.

It verifies what it imported rather than trusting the importer: a table that lands
with zero rows is reported as a failure, because that is exactly what a wrong row
struct looks like.
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


def build_task(source_dir, asset_name, struct_name):
    json_path = os.path.abspath(os.path.join(source_dir, asset_name + ".json"))
    if not os.path.exists(json_path):
        unreal.log_error("[import] no such file: %s" % json_path)
        return None

    struct = unreal.load_object(None, MODULE + struct_name)
    if struct is None:
        unreal.log_error(
            "[import] row struct %s not found -- has the module compiled?" % struct_name)
        return None

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    settings.set_editor_property("import_row_struct", struct)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", json_path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", factory)
    return task


def verify(asset_name):
    """A table that imported with no rows is a wrong row struct, not a success."""
    path = "%s/%s.%s" % (DEST, asset_name, asset_name)
    table = unreal.load_object(None, path)
    if table is None:
        return 0
    return len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))


def main():
    source_dir = os.path.join(unreal.Paths.project_dir(), "Content", "Data")
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    tasks = []
    for asset_name, struct_name in MAPPING:
        task = build_task(source_dir, asset_name, struct_name)
        if task is not None:
            tasks.append(task)

    if tasks:
        tools.import_asset_tasks(tasks)

    unreal.log("[import] ---- results ----")
    total, failed = 0, []
    for asset_name, struct_name in MAPPING:
        rows = verify(asset_name)
        total += rows
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
