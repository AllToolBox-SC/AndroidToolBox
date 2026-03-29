use std::env;
use std::process;
use std::fs;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() > 3 {
        let mode = &args[1];
        let json = &args[2];
        let key = &args[3];

        match mode.as_str() {
            "getfile" => {
                let content = fs::read_to_string(json).expect("Failed to read JSON file");
                if content == "Failed to read JSON file" {
                    eprintln!("Failed to read JSON file");
                    process::exit(1);
                } else {
                    let json_data: serde_json::Value = serde_json::from_str(&content).expect("Invalid JSON format");
                    // Get Key from var key
                    if let Some(value) = json_data.get(key) {
                        println!("{}", value);
                    } else {
                        eprintln!("Key not found");
                    }
                }
            }
            "getstr" => {
                // To Json object from var json
                let json_data: serde_json::Value = serde_json::from_str(json).expect("Invalid JSON format");
                // Get Key from var key
                if let Some(value) = json_data.get(key) {
                    println!("{}", value);
                } else {
                    eprintln!("Key not found");
                    process::exit(1);
                }
            }
            _ => eprintln!("Unknown mode: {}", mode),
        }
    }
    else {
        eprintln!("Usage: <program> <mode> <json> <key>");
    }
}