extern crate serde;
extern crate serde_json;

use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
struct Person {
    name: String,
    age: u8,
}

// Function hello_rust_cargo without manglng
#[no_mangle]
pub extern "C" fn hello_rust_cargo_main() {
    // Print hello world to stdout

    let john = Person {
        name: "John".to_string(),
        age: 30,
    };

    let json_str = serde_json::to_string(&john).unwrap();
    println!("{}", json_str);

    let jane = Person {
        name: "Jane".to_string(),
        age: 25,
    };

    let json_str_jane = serde_json::to_string(&jane).unwrap();
    println!("{}", json_str_jane);

    let json_data = r#"
        {
            "name": "Alice",
            "age": 28
        }"#;

    let alice: Person = serde_json::from_str(json_data).unwrap();
    println!("Deserialized: {} is {} years old", alice.name, alice.age);

    let pretty_json_str = serde_json::to_string_pretty(&alice).unwrap();
    println!("Pretty JSON:\n{}", pretty_json_str);

    std::thread::spawn(|| loop {
        println!("Hello world from thread! {:?}", std::thread::current().id());
        std::thread::sleep(std::time::Duration::from_secs(3))
    });

    tokio::runtime::Builder::new_current_thread()
        // .enable_all()
        .enable_time()
        .build()
        .unwrap()
        .block_on(async {
            tokio::join!(
                async {
                    loop {
                        println!(
                            "Hello world from tokio 1! {:?}",
                            std::thread::current().id()
                        );
                        tokio::time::sleep(tokio::time::Duration::from_secs(2)).await;
                    }
                },
                async {
                    loop {
                        println!(
                            "Hello world from tokio 2! {:?}",
                            std::thread::current().id()
                        );
                        tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
                    }
                }
            );
        });

    loop {
        // Do nothing
    }
}
