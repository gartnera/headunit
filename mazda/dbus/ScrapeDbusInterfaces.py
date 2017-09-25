#!/usr/bin/python3
import os
import sys
import fnmatch
import xml.dom.minidom

def isEmptyNode(node):
    return node.nodeType == node.TEXT_NODE and node.data.strip() == ''

def removeEmptyNodes(parent):
    if parent.childNodes:
       for child in list(parent.childNodes):
            if isEmptyNode(child):
                parent.removeChild(child)
                continue
            removeEmptyNodes(child)

#annotations with no value crash gdbus-codegen
def fixInvalidAttrs(parent):
    for child in parent.getElementsByTagName("annotation"):
        if not child.hasAttribute("value"):
            child.setAttribute("value", "")

def fixConflictingArgNames(parent):
    def uncap(s):
        return s[0].lower() + s[1:]

    for child in parent.getElementsByTagName("arg"):
        if child.hasAttribute("name"):
            child.setAttribute("name", uncap(child.getAttribute("name")))

#skip this interface since is has a struct with >16 params
seenInterfaces = { "com.jci.msgs.Client", "com.jci.vbs.vwm", "com.jci.vbs.settings", "com.jci.obs.aha.hmi" }
outputDoc = xml.dom.minidom.Document()
outputNode = outputDoc.createElement("node")

def scanFile(filename):
    print("Reading {0}".format(filename))
    with open(filename, "rb") as f:
        filedata = f.read()
        curPos = None
        nextStart = -1
        while True:
            nextStart = filedata.find(b"<interface ", curPos)
            if nextStart < 0:
                break
            endTag = b"</interface>"
            lastTagStart = filedata.find(endTag, nextStart)
            if lastTagStart <= nextStart:
                break
            curPos = lastTagStart + len(endTag)
            xmlStr = filedata[nextStart:curPos].decode("utf-8")
            try:
                xmlData = xml.dom.minidom.parseString(xmlStr)
                ifaceName = xmlData.documentElement.getAttribute("name")
                if ifaceName not in seenInterfaces:
                    print("Found {0}".format(ifaceName))
                    seenInterfaces.add(ifaceName)
                    removeEmptyNodes(xmlData)
                    fixInvalidAttrs(xmlData)
                    fixConflictingArgNames(xmlData)
                    outputNode.appendChild(xmlData.documentElement)
            except Exception as e:
                print(e)
                print(xmlStr)
    print("Done")

startDir = sys.argv[1]
print(startDir)
for root, dirnames, filenames in os.walk(startDir):
    for filename in filenames:
        if filename.endswith(".lua") or filename.endswith(".so") or filename.find(".") == -1:
            scanFile(os.path.join(root, filename))

prettyString = outputNode.toprettyxml();
with open(sys.argv[2], "w") as o:
    o.write(prettyString)
