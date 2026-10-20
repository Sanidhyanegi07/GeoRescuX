import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GeoRescuX.Prototype 1.0

Rectangle {
    id: userMapScreen
    color: "#080C14"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top Control Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#0F172A"
            border.color: "#1E293B"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "🗺️ " + (appState.activeRegionDisplayName.length > 0 ? appState.activeRegionDisplayName : "Offline Map")
                    color: "#F8FAFC"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                Rectangle {
                    height: 22
                    width: offBadge.contentWidth + 12
                    radius: 11
                    color: appState.hasActiveMapPackage ? "#064E3B" : "#7F1D1D"
                    border.color: appState.hasActiveMapPackage ? "#059669" : "#DC2626"
                    border.width: 1
                    Text {
                        id: offBadge
                        anchors.centerIn: parent
                        text: appState.hasActiveMapPackage ? "100% OFFLINE (0 NET)" : "NO PACKAGE"
                        color: appState.hasActiveMapPackage ? "#A7F3D0" : "#FECACA"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }
                }

                // Live GPS Tracking & Coverage Chip
                Rectangle {
                    height: 22
                    width: gpsBadgeText.contentWidth + 14
                    radius: 11
                    color: {
                        if (!appState.locationService) return "#1E293B";
                        if (appState.locationService.isOutsideCoverage) return "#7C2D12";
                        if (appState.locationService.hasValidFix) return "#064E3B";
                        if (appState.locationService.isStale) return "#78350F";
                        if (appState.locationService.permissionStatusName === "Denied") return "#7F1D1D";
                        return "#1E293B";
                    }
                    border.color: {
                        if (!appState.locationService) return "#334155";
                        if (appState.locationService.isOutsideCoverage) return "#EA580C";
                        if (appState.locationService.hasValidFix) return "#059669";
                        if (appState.locationService.isStale) return "#D97706";
                        if (appState.locationService.permissionStatusName === "Denied") return "#DC2626";
                        return "#334155";
                    }
                    border.width: 1

                    Text {
                        id: gpsBadgeText
                        anchors.centerIn: parent
                        text: {
                            if (!appState.locationService) return "GPS OFF";
                            if (appState.locationService.isOutsideCoverage) return "⚠️ OUTSIDE REGION";
                            if (appState.locationService.hasValidFix) return "🛰️ GPS ±" + appState.locationService.currentAccuracyMeters.toFixed(1) + "m";
                            if (appState.locationService.isStale) return "⚠️ GPS STALE";
                            if (appState.locationService.permissionStatusName === "Denied") return "❌ NO PERMISSION";
                            return "🛰️ ACQUIRING GPS...";
                        }
                        color: {
                            if (!appState.locationService) return "#94A3B8";
                            if (appState.locationService.isOutsideCoverage) return "#FDBA74";
                            if (appState.locationService.hasValidFix) return "#A7F3D0";
                            if (appState.locationService.isStale) return "#FDE68A";
                            if (appState.locationService.permissionStatusName === "Denied") return "#FECACA";
                            return "#93C5FD";
                        }
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (appState.locationService && !appState.locationService.hasValidFix) {
                                appState.locationService.requestPermission();
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Quick reset view button
                Button {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: 64
                    background: Rectangle {
                        radius: 6
                        color: "#1E293B"
                        border.color: "#334155"
                    }
                    contentItem: Text {
                        text: "⟲ Reset"
                        color: "#CBD5E1"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: userMapView.resetView()
                }
            }
        }

        // Map Container
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            OfflineMapView {
                id: userMapView
                anchors.fill: parent

                Component.onCompleted: {
                    reloadMap();
                    syncUserLocation();
                }

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

                function syncUserLocation() {
                    var loc = appState.locationService;
                    if (loc && (loc.hasValidFix || loc.currentLatitude !== 0.0)) {
                        userMapView.setUserPosition(
                            loc.currentLatitude,
                            loc.currentLongitude,
                            loc.currentAccuracyMeters,
                            loc.hasValidFix,
                            loc.isStale
                        );
                    } else {
                        userMapView.clearUserPosition();
                    }
                }

                Connections {
                    target: appState
                    function onActiveRegionChanged() { userMapView.reloadMap(); }
                }

                Connections {
                    target: routeService
                    function onRouteChanged() {
                        if (routeService && routeService.hasRoute) {
                            userMapView.setRouteCoordinates(routeService.lastCoordinates, routeService.lastDistanceMeters);
                        } else {
                            userMapView.clearRouteOverlay();
                        }
                    }
                }

                Connections {
                    target: appState.locationService
                    function onPositionChanged() { userMapView.syncUserLocation(); }
                    function onStateChanged() { userMapView.syncUserLocation(); }
                }
            }

            // Floating Pan, Zoom & GPS Location Controls (Top Right)
            ColumnLayout {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 6

                Rectangle {
                    width: 36; height: 36; radius: 8
                    color: "#CC0F172A"; border.color: "#334155"; border.width: 1
                    Text { anchors.centerIn: parent; text: "➕"; color: "#FFFFFF"; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: userMapView.zoomIn() }
                }

                Rectangle {
                    width: 36; height: 36; radius: 8
                    color: "#CC0F172A"; border.color: "#334155"; border.width: 1
                    Text { anchors.centerIn: parent; text: "➖"; color: "#FFFFFF"; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: userMapView.zoomOut() }
                }

                // Center on User GPS Position Button
                Rectangle {
                    width: 36; height: 36; radius: 8
                    color: appState.locationService && appState.locationService.hasValidFix ? "#064E3B" : "#CC0F172A"
                    border.color: appState.locationService && appState.locationService.hasValidFix ? "#059669" : "#334155"
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "⌖"
                        color: appState.locationService && appState.locationService.hasValidFix ? "#34D399" : "#64748B"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (appState.locationService && appState.locationService.hasValidFix) {
                                userMapView.centerOn(appState.locationService.currentLatitude, appState.locationService.currentLongitude);
                            } else if (appState.locationService) {
                                appState.locationService.requestPermission();
                            }
                        }
                    }
                }

                Rectangle {
                    width: 36; height: 24; radius: 6
                    color: "#CC0F172A"; border.color: "#334155"; border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: Math.round(userMapView.zoomLevel) + "x"
                        color: "#94A3B8"; font.pixelSize: 9; font.family: "monospace"
                    }
                }
            }

            // Floating Metrics HUD (Bottom Left)
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 12
                width: 190
                height: 60
                radius: 8
                color: "#E60F172A"
                border.color: "#1E293B"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2

                    Text {
                        text: "🛣️ Roads: " + userMapView.renderedRoadCount + " | 🏥 POIs: " + userMapView.renderedPlaceCount
                        color: "#38BDF8"; font.pixelSize: 9; font.weight: Font.Bold
                    }
                    Text {
                        text: "Center: " + userMapView.centerLatitude.toFixed(3) + "°N, " + userMapView.centerLongitude.toFixed(3) + "°E"
                        color: "#94A3B8"; font.pixelSize: 8; font.family: "monospace"
                    }
                    Text {
                        text: "Drag: Pan | Wheel: Zoom | Zero Cloud"
                        color: "#64748B"; font.pixelSize: 8
                    }
                }
            }

            // Floating Legend HUD (Bottom Right)
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 12
                width: 140
                height: 72
                radius: 8
                color: "#E60F172A"
                border.color: "#1E293B"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2

                    Text { text: "🟡 State Boundary"; color: "#FDE68A"; font.pixelSize: 8 }
                    Text { text: "🟠 Road Network"; color: "#FB923C"; font.pixelSize: 8 }
                    Text { text: "🟢 Safe Havens"; color: "#34D399"; font.pixelSize: 8 }
                    Text { text: "🔵 Active Route"; color: "#60A5FA"; font.pixelSize: 8 }
                    Text { text: "🟣 My GPS Fix"; color: "#38BDF8"; font.pixelSize: 8 }
                }
            }
        }
    }
}
