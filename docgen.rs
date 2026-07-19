use std::io;
use std::fs;
use std::collections::BTreeMap;
use std::env;

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
    let args = env::args();
    let filename = if args.len() > 1 {
        args.skip(1).next().expect("OOPS").to_string()
    } else {
        "input.txt".to_string()
    };
    let data = fs::read(filename)?;
    let mut mode;
    let mut buf: Vec<_> = vec![];
    let mut obj: Vec<Object> = vec![];
    let mut labels: BTreeMap<String, usize> = BTreeMap::default();

    mode = ParserMode::PRINT;

    for i in 0..=data.len() {
        let c = if i == data.len() { b' ' } else { data[i] };
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

        if c == b'@' || i == data.len() {
            let str = match String::from_utf8(buf.clone()) {
                Ok(s) => s,
                Err(_) => panic!("oops could not decode"),
            };
            match mode {
                ParserMode::LABEL => {
                    let str = str.trim().to_string().replace("_", "\\_").replace("#", "\\#");
                    labels.insert(str.clone(), labels.len());
                    obj.push(Object::Label(str));
                },
                ParserMode::COMMENT => {
                    let str = str.trim().to_string().replace("_", "\\_").replace("#", "\\#");
                    obj.push(Object::Comment(str));
                },
                ParserMode::MEMWRITE => {
                    let str = str.trim().to_string().replace("_", "\\_").replace("#", "\\#").replace("\n", "\\smallskip\n");
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

    for (k, v) in labels.iter() {
        println!("\\tocent{{{}}}{{{}}}" , k, v);
    }

    for o in obj {
        match o {
            Object::Label(s) => {
                if let Some(&r) = labels.get(&s) {
                    println!("\\xrdef{{{}}}" , r);
                }
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
