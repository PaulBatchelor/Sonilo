func cmd(name) {
    return "\\"name
}

BEGIN {
    codeblock=0
    print(cmd("pdfpagewidth=148mm"))
    print(cmd("pdfpageheight=210mm"))
    print(cmd("hsize=128mm"))
    print(cmd("vsize=190mm"))
    print(cmd("pdfhorigin=0pt"))
    print(cmd("pdfvorigin=0pt"))
    print(cmd("hoffset=10mm"))
    print(cmd("voffset=10mm"))
}

/~~~/ {
    if (codeblock) {
        codeblock = 0
        print cmd("medskip")
        print cmd("hrule")
        print cmd("medskip")
        print cmd("endgroup")
    } else {
        codeblock = 1
        print cmd("begingroup") cmd("tt") cmd("parindent=0pt")
        print cmd("medskip")
        print cmd("hrule")
        print cmd("medskip")
    }
    next
}

{
    if (codeblock) {
        gsub(/#/, "\\#") 
    }
    print $0
}

END {
    print cmd("bye")
}
