use std::io;
use std::fs;
use std::collections::{BTreeMap, HashMap, BTreeSet};
use std::error::Error;

#[derive(PartialEq)]
enum ParserMode {
    PRINT,
    COMMENT,
    MEMWRITE,
    UPDATE,
    LABEL,
}

enum Object {
    Label(String),
    Comment(String),
    Code(String),
}

fn main() -> io::Result<()> {
    let filename = "input.txt";
    let data = fs::read(filename)?;
    let mut mode;
    let mut buf: Vec<_> = vec![];
    let mut obj: Vec<Object> = vec![];

    mode = ParserMode::PRINT;

    for c in data {
        if matches!(mode, ParserMode::UPDATE) {
            if c == b'l' {
                mode = ParserMode::LABEL;
            } else if c == b'c' {
                mode = ParserMode::COMMENT;
            } else if c == b'm' {
                mode = ParserMode::MEMWRITE;
            } else if c == b'p' {
                mode = ParserMode::PRINT;
            }
            buf.clear();
            continue
        }

        // TODO: check for terminal character?
        if c == b'@' {
            let str = match String::from_utf8(buf.clone()) {
                Ok(s) => s,
                Err(_) => panic!("oops could not decode"),
            };
            let str = str.trim().to_string().replace("_", "\\_").replace("#", "\\#");
            match mode {
                ParserMode::LABEL => {
                    obj.push(Object::Label(str));
                },
                ParserMode::COMMENT => {
                    obj.push(Object::Comment(str));
                },
                ParserMode::MEMWRITE => {
                    obj.push(Object::Code(str));
                }
                _ => {},
            }
            mode = ParserMode::UPDATE;
            continue;
        }

        match mode {
            ParserMode::PRINT => {},
            ParserMode::UPDATE => {},
            ParserMode::MEMWRITE => {
                buf.push(c)
            },
            ParserMode::COMMENT=> {
                buf.push(c)
            },
            ParserMode::LABEL => {
                buf.push(c)
            },
        };
    }

    // TODO: process remaining things in buffer, or add
    // some kind of terminal code in loop above

    for o in obj {
        match o {
            Object::Label(s) => {
                println!("\\label{{{}}}" , s);
            },
            Object::Code(s) => {
                println!("\\code{{{}}}", s.replace("$", "\\$"));
            },
            Object::Comment(s) => {
                println!("\\comment{{{}}}" , s);
            }
        };
    }

    Ok(())
}
