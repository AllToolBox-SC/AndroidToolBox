import os

if __name__ != "__main__":
    # If this module is imported, set the debug mode environment variable
    # This allows the main script to detect if debug mode is enabled
    os.environ["ATB_DEBUG_MODE"] = "1"
