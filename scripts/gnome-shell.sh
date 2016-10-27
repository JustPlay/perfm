#!/bin/bash

    # Modify gnome-shell

    # should NOT use 'PWD'
    _PWD_=/tmp/gnome-shell
    _GST_=/usr/share/gnome-shell/gnome-shell-theme.gresource
    THEME=theme

    rm    -rf ${_PWD_}
    mkdir -p  ${_PWD_}
    cd        ${_PWD_}

    # 1) Extract gnome-shell-theme.gresource into $_PWD_/$THEME
    mkdir $THEME

    for path in $(gresource list ${_GST_}); do
        file=$(echo ${path} | sed -e 's/^\/org\/gnome\/shell\/theme\///g')    
        gresource extract ${_GST_} ${path} > ${_PWD_}/${THEME}/${file}
    done

    # 2) Create gnome-shell-theme.gresource.xml inside $_PWD_/$THEME
    XML_FILE=${_PWD_}/${THEME}/gnome-shell-theme.gresource.xml

    echo "<?xml version="1.0" encoding="UTF-8"?>"          >> ${XML_FILE}
    echo "<gresources>"                                    >> ${XML_FILE}
    echo "  <gresource prefix=\"/org/gnome/shell/theme\">" >> ${XML_FILE}

    for path in $(gresource list ${_GST_}); do
        file=$(echo ${path} | sed -e 's/^\/org\/gnome\/shell\/theme\///g')    
        echo "    <file>${file}</file>"                      >> ${XML_FILE}
    done
    
    echo "  </gresource>"                                   >> ${XML_FILE}
    echo "</gresources>"                                   >> ${XML_FILE}

    # 3) do some configution (e.g. modify gnome-shell.css)
    read -n 1 -p "Please edit [gnome-shell.css], And then press any key..."

    # 4) build
    cd $_PWD_/$THEME
    glib-compile-resources $(basename ${XML_FILE})
