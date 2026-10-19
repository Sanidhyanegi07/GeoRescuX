import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 420
    height: 780
    minimumWidth: 360
    minimumHeight: 640
    title: qsTr("GeoRescuX Qt Prototype")
    color: "#0B0F19" // Deep midnight slate

    property string latestDetails: "No calculation executed yet. Tap button below to test native C++20 routine."
    property string lastDistanceStr: "0.0 m"

    Connections {
        target: prototypeBackend

        function onCalculationFinished(distanceMeters, details) {
            root.lastDistanceStr = distanceMeters.toFixed(1) + " m";
            root.latestDetails = details;
        }

        function onLogMessageEmitted(formattedMessage) {
            logModel.append({ message: formattedMessage });
            if (logListView.count > 0) {
                logListView.positionViewAtEnd();
            }
        }
    }

    ListModel {
        id: logModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        // Top App Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: 12
            color: "#161F30"
            border.color: "#24344D"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 40
                    height: 40
                    radius: 8
                    color: "#0284C7" // Rescue blue
                    Text {
                        anchors.centerIn: parent
                        text: "🛡️"
                        font.pixelSize: 22
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "GeoRescuX Qt Prototype"
                        color: "#F8FAFC"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Stage 1 Validation — com.example.georescux.qtprototype"
                        color: "#94A3B8"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Status & Target Metadata Badges
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                radius: 6
                color: "#111827"
                border.color: "#374151"
                border.width: 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#10B981" // Emerald active dot
                    }
                    Text {
                        text: "Target: API 28+ (C++20)"
                        color: "#D1D5DB"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                radius: 6
                color: "#111827"
                border.color: "#374151"
                border.width: 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#38BDF8"
                    }
                    Text {
                        text: "Safe Sandbox Coexistence"
                        color: "#D1D5DB"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }
        }

        // Native Benchmark Action Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            radius: 12
            color: "#1E293B"
            border.color: "#334155"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "Native C++20 Geodesic Engine"
                    color: "#F1F5F9"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Text {
                    text: "Invokes `georescux::spatial::GeodesicMath` to compute WGS-84 Vincenty & Haversine distances between Dehradun and Rishikesh corridor."
                    color: "#94A3B8"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Interactive Trigger Button
                Button {
                    id: testButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46

                    background: Rectangle {
                        radius: 8
                        color: testButton.down ? "#0369A1" : (testButton.hovered ? "#0284C7" : "#0EA5E9")
                    }

                    contentItem: Text {
                        text: "▶  Execute C++ Geodesic Benchmark"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        prototypeBackend.runGeodesicBenchmark();
                    }
                }

                // Status indicator
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: "Engine State:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: prototypeBackend.statusText
                        color: "#38BDF8"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Live Computation Result Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            radius: 12
            color: "#132137"
            border.color: "#1E3A5F"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Last Computed Geodesic Distance:"
                        color: "#94A3B8"
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.lastDistanceStr
                        color: "#34D399" // Mint green
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#1E3A5F"
                }

                Text {
                    text: root.latestDetails
                    color: "#E2E8F0"
                    font.pixelSize: 11
                    font.family: "monospace"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Live Runtime Event Console
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#080C14"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Runtime Event & Lifecycle Console"
                        color: "#9CA3AF"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Count: " + prototypeBackend.triggerCount
                        color: "#6B7280"
                        font.pixelSize: 11
                    }
                }

                ListView {
                    id: logListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: logModel

                    delegate: Item {
                        width: logListView.width
                        height: logItemText.paintedHeight + 4

                        Text {
                            id: logItemText
                            width: parent.width
                            text: model.message
                            color: "#A7F3D0"
                            font.pixelSize: 11
                            font.family: "monospace"
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
