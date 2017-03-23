import QtQuick 2.0
import Sailfish.Silica 1.0
import "."

ListItem {
    id: listItem
    width: listView.width
    ListView.onRemove: animateRemoval(listItem)

    onClicked: pageStack.push(Qt.resolvedUrl("BeerPage.qml"), { "model": model })

    Label {
        id: listLabel
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        text: name
        color: contextMenu.active ? Theme.highlightColor : Theme.primaryColor
    }

    Row {
        id: row
        width: parent.width / 3
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        Repeater {
            id: repeater
            model: 5

            GlassItem {
                color: "white"
                width: parent.width / 5
                height: parent.width / 5
                radius: 4
                falloffRadius: 0.2
                visible: (rating > index) ? true : false
            }
        }
    }

    menu: ContextMenu {
        id: contextMenu

        MenuItem {
            text: qsTr("Remove")
            onClicked: remorseAction(qsTr("Deleting"), function() {
                BeerModel.removeBeer(index);
            })
        }
    }
}