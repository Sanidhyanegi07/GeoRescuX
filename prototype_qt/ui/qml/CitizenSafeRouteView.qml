import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GeoRescuX.Prototype 1.0

Rectangle {
    id: userSafeRouteScreen
    color: "#080C14"

    property var startWaypoints: [
        { name: "Dehradun (ISBT / Center)", lat: 30.3165, lon: 78.0322 },
        { name: "Haridwar (Bypass Road)", lat: 29.9457, lon: 78.1642 },
        { name: "Rishikesh (Triveni Ghat)", lat: 30.1058, lon: 78.2880 },
        { name: "Srinagar Garhwal", lat: 30.2228, lon: 78.7847 }
    ]

    property var destWaypoints: [
        { name: "Rishikesh (AIIMS Safe Haven)", lat: 30.1058, lon: 78.2880 },
        { name: "Dehradun (District Hospital)", lat: 30.3165, lon: 78.0322 },
        { name: "Roorkee (Safe Zone)", lat: 29.8543, lon: 77.8880 },
        { name: "Uttarkashi (Valley Evacuation)", lat: 30.7268, lon: 78.4354 }
    ]

    property int selectedStartIdx: 0
    property int selectedDestIdx: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Header
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "🧭 Offline Safe Route Engine"
                color: "#F8FAFC"
                font.pixelSize: 15
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                height: 20
                width: aStarBadge.contentWidth + 10
                radius: 10
                color: "#1E1B4B"
                border.color: "#6366F1"
                border.width: 1
                Text {
                    id: aStarBadge
                    anchors.centerIn: parent
                    text: "C++20 A* PATHFINDER"
                    color: "#C7D2FE"
                    font.pixelSize: 8
                    font.weight: Font.Bold
                }
            }
        }

        // Waypoint Selection Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: waypointCol.implicitHeight + 20
            radius: 10
            color: "#0F172A"
            border.color: "#1E293B"
            border.width: 1

            ColumnLayout {
                id: waypointCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: "🟢 Start:"; color: "#34D399"; font.pixelSize: 11; font.weight: Font.Bold; Layout.preferredWidth: 60 }
                    ComboBox {
                        id: startCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        model: [
                            "Dehradun (ISBT / Center) [30.316, 78.032]",
                            "Haridwar (Bypass Road) [29.945, 78.164]",
                            "Rishikesh (Triveni Ghat) [30.105, 78.288]",
                            "Srinagar Garhwal [30.222, 78.784]"
                        ]
                        currentIndex: userSafeRouteScreen.selectedStartIdx
                        onCurrentIndexChanged: userSafeRouteScreen.selectedStartIdx = currentIndex
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: "🔴 Target:"; color: "#F87171"; font.pixelSize: 11; font.weight: Font.Bold; Layout.preferredWidth: 60 }
                    ComboBox {
                        id: destCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        model: [
                            "Rishikesh (AIIMS Safe Haven) [30.105, 78.288]",
                            "Dehradun (District Hospital) [30.316, 78.032]",
                            "Roorkee (Safe Zone) [29.854, 77.888]",
                            "Uttarkashi (Valley Evacuation) [30.726, 78.435]"
                        ]
                        currentIndex: userSafeRouteScreen.selectedDestIdx
                        onCurrentIndexChanged: userSafeRouteScreen.selectedDestIdx = currentIndex
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            radius: 6
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#0284C7" }
                                GradientStop { position: 1.0; color: "#0369A1" }
                            }
                        }
                        contentItem: Text {
                            text: routeService.isCalculating ? "Calculating Route..." : "▶ Calculate Evacuation Route"
                            color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            const s = userSafeRouteScreen.startWaypoints[userSafeRouteScreen.selectedStartIdx];
                            const d = userSafeRouteScreen.destWaypoints[userSafeRouteScreen.selectedDestIdx];
                            appState.calculateRoute(s.lat, s.lon, d.lat, d.lon);
                        }
                    }

                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            radius: 6
                            color: "#1E293B"
                            border.color: "#334155"
                        }
                        contentItem: Text {
                            text: "Clear"
                            color: "#CBD5E1"; font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: appState.clearRoute()
                    }
                }
            }
        }

        // Route Map Preview (Middle)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: "#0B1120"
            border.color: "#1E293B"
            clip: true

            OfflineMapView {
                id: routePreviewMap
                anchors.fill: parent

                Component.onCompleted: reloadMap()

                function reloadMap() {
                    if (appState.hasActiveMapPackage) {
                        loadRegionData(
                            appState.activeRegionId,
                            appState.activeRegionDisplayName,
                            appState.activeBoundaryPath,
                            appState.activeGraphPath,
                            appState.activePlacesPath
                        );
                        if (routeService && routeService.hasRoute) {
                            setRouteCoordinates(routeService.lastCoordinates, routeService.lastDistanceMeters);
                        }
                    }
                }

                Connections {
                    target: appState
                    function onActiveRegionChanged() { routePreviewMap.reloadMap(); }
                }

                Connections {
                    target: routeService
                    function onRouteChanged() {
                        if (routeService && routeService.hasRoute) {
                            routePreviewMap.setRouteCoordinates(routeService.lastCoordinates, routeService.lastDistanceMeters);
                        } else {
                            routePreviewMap.clearRouteOverlay();
                        }
                    }
                }
            }

            // Map overlay badge showing calculated distance
            Rectangle {
                visible: routeService.hasRoute
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 8
                height: 28
                width: distText.contentWidth + 16
                radius: 14
                color: "#EE0369A1"
                border.color: "#38BDF8"
                border.width: 1
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        id: distText
                        text: "Route: " + routeService.lastDistanceFormatted + " (" + routeService.lastDurationMinutes + " min)"
                        color: "#FFFFFF"; font.pixelSize: 10; font.weight: Font.Bold
                    }
                }
            }
        }

        // Route Results & Guidance Drawer (Bottom)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            radius: 10
            color: "#0F172A"
            border.color: "#1E293B"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "STATUS: " + routeService.lastStatusMessage
                        color: routeService.hasRoute ? "#34D399" : "#FBBF24"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Steps: " + routeService.lastSteps.length
                        color: "#94A3B8"; font.pixelSize: 10
                    }
                }

                // Step-by-Step Instructions List
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: stepList
                        width: parent.width
                        model: routeService.lastSteps
                        spacing: 4
                        delegate: Rectangle {
                            width: stepList.width
                            height: 24
                            radius: 4
                            color: "#1E293B"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 6
                                Text {
                                    text: "#" + modelData.stepNumber
                                    color: "#38BDF8"; font.pixelSize: 9; font.weight: Font.Bold
                                }
                                Text {
                                    text: modelData.instruction
                                    color: "#E2E8F0"; font.pixelSize: 9
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.distanceMeters + " m"
                                    color: "#94A3B8"; font.pixelSize: 9; font.family: "monospace"
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "⚠️ Safety Note: Offline topological shortest path. Does not guarantee dynamic hazard avoidance."
                    color: "#64748B"; font.pixelSize: 8
                }
            }
        }
    }
}
