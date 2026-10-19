import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 480
    height: 840
    minimumWidth: 380
    minimumHeight: 680
    title: qsTr("GeoRescuX — Native C++20 Disaster Engine")
    color: "#080C14" // Deep midnight slate

    property int activeTab: 0

    // Routing state
    property string routePathStr: "Dehradun ISBT ➔ Jolly Grant ➔ Rishikesh Camp"
    property string routeStatsStr: "Ready. Tap 'Calculate A* Evacuation Route' to compute."
    property string routeWarningsStr: "None"

    // Mesh state
    property string packetHexStr: "013f00018f4a... (24 bytes)"
    property string hmacSigStr: "Waiting for transmission..."
    property string meshDecodedStr: "No active packet"
    property bool crcValid: true

    // Triage state
    property string triageCategory: "IMMEDIATE"
    property string triageColor: "#EF4444"
    property int triageScore: 92
    property string triageReason: "Uncontrolled hemorrhage + trapped in collapse"
    property string triageAction: "Apply high-pressure tourniquet, structural shoring"
    property real survivalPercent: 74.5

    // Spatial & Telemetry state
    property string nearestShelter: "Jolly Grant Trauma Safe Haven"
    property real shelterDistKm: 18.4
    property string fusedLocationStr: "30.31650° N, 78.03220° E (±2.5m)"
    property real fusedSpeed: 4.8

    Connections {
        target: prototypeBackend

        function onRouteCalculated(nodePath, distanceMeters, costMeters, durationSeconds, warnings) {
            root.routePathStr = nodePath;
            const distKm = (distanceMeters / 1000.0).toFixed(2);
            const costKm = (costMeters / 1000.0).toFixed(2);
            const mins = Math.round(durationSeconds / 60);
            root.routeStatsStr = "Distance: " + distKm + " km | Hazard Cost: " + costKm + " km | Est: " + mins + " min";
            root.routeWarningsStr = warnings;
        }

        function onMeshPacketProcessed(packetHex, sizeBytes, crcOk, hmacDigest, decodedSummary) {
            root.packetHexStr = packetHex;
            root.crcValid = crcOk;
            root.hmacSigStr = hmacDigest;
            root.meshDecodedStr = decodedSummary;
        }

        function onTriageEvaluated(category, score, reason, intervention, survivalProb) {
            root.triageCategory = category;
            root.triageScore = score;
            root.triageReason = reason;
            root.triageAction = intervention;
            root.survivalPercent = (survivalProb * 100.0).toFixed(1);

            if (category.indexOf("RED") !== -1 || category.indexOf("IMMEDIATE") !== -1) {
                root.triageColor = "#EF4444";
            } else if (category.indexOf("YELLOW") !== -1 || category.indexOf("DELAYED") !== -1) {
                root.triageColor = "#F59E0B";
            } else if (category.indexOf("GREEN") !== -1 || category.indexOf("MINOR") !== -1) {
                root.triageColor = "#10B981";
            } else {
                root.triageColor = "#6B7280";
            }
        }

        function onShelterFound(shelterName, lat, lon, distanceMeters) {
            root.nearestShelter = shelterName;
            root.shelterDistKm = (distanceMeters / 1000.0).toFixed(2);
        }

        function onLocationFused(lat, lon, speedMps, accuracyMeters) {
            root.fusedLocationStr = lat.toFixed(5) + "° N, " + lon.toFixed(5) + "° E (±" + accuracyMeters.toFixed(1) + "m)";
            root.fusedSpeed = speedMps.toFixed(1);
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
        anchors.margins: 14
        spacing: 10

        // =====================================================================
        // Header Bar
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 12
            color: "#111827"
            border.color: "#1F2937"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                Rectangle {
                    width: 42
                    height: 42
                    radius: 10
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0284C7" }
                        GradientStop { position: 1.0; color: "#0369A1" }
                    }
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
                        text: "GeoRescuX"
                        color: "#F9FAFB"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Native C++20 Mission-Critical Engine"
                        color: "#38BDF8"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 90
                    height: 26
                    radius: 13
                    color: "#064E3B"
                    border.color: "#059669"
                    border.width: 1
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: "#34D399"
                        }
                        Text {
                            text: "C++ ACTIVE"
                            color: "#A7F3D0"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Subsystem Tab Navigation Bar
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { name: "Routing", icon: "🗺️" },
                    { name: "Mesh", icon: "📡" },
                    { name: "Triage", icon: "🚑" },
                    { name: "Spatial", icon: "🛰️" },
                    { name: "Console", icon: "📋" }
                ]

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 8
                    color: root.activeTab === index ? "#1E293B" : "#0F172A"
                    border.color: root.activeTab === index ? "#38BDF8" : "#1E293B"
                    border.width: root.activeTab === index ? 1.5 : 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: modelData.icon
                            font.pixelSize: 13
                        }
                        Text {
                            text: modelData.name
                            color: root.activeTab === index ? "#F8FAFC" : "#94A3B8"
                            font.pixelSize: 11
                            font.weight: root.activeTab === index ? Font.Bold : Font.Normal
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = index
                    }
                }
            }
        }

        // =====================================================================
        // Tab 0: Evacuation Routing Engine (A* / Hazards)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 0
            radius: 12
            color: "#111827"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "A* Offline Evacuation Routing"
                    color: "#F9FAFB"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Text {
                    text: "Evaluates Uttarakhand road network graph with real-time landslide/flood hazard penalties using C++20 `AStarPathfinder`."
                    color: "#94A3B8"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: hazardCheck
                        checked: true
                        text: "Avoid Active Hazards (Landslide Flood Bypass)"
                        contentItem: Text {
                            text: hazardCheck.text
                            font.pixelSize: 12
                            color: "#E2E8F0"
                            leftPadding: hazardCheck.indicator.width + 4
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Button {
                    id: routeBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    background: Rectangle {
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: routeBtn.down ? "#0369A1" : "#0284C7" }
                            GradientStop { position: 1.0; color: "#075985" }
                        }
                    }
                    contentItem: Text {
                        text: "▶  Calculate A* Evacuation Route"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        prototypeBackend.runAStarRoutingBenchmark(hazardCheck.checked);
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0B0F19"
                    border.color: "#1F2937"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "OPTIMAL DISASTER CORRIDOR:"
                            color: "#64748B"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }

                        Text {
                            text: root.routePathStr
                            color: "#38BDF8"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#1F2937"
                        }

                        Text {
                            text: root.routeStatsStr
                            color: "#34D399"
                            font.pixelSize: 11
                            font.family: "monospace"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Hazard Status: " + root.routeWarningsStr
                            color: "#FBBF24"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Tab 1: BLE Mesh Packet Codec & HMAC Authenticator
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 1
            radius: 12
            color: "#111827"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "BLE Mesh SOS Protocol & Cryptography"
                    color: "#F9FAFB"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Text {
                    text: "Generates 24-byte bit-packed emergency packets with CRC-16-CCITT and FIPS-180-4 HMAC-SHA256 anti-spoof authentication."
                    color: "#94A3B8"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text { text: "Victims:"; color: "#CBD5E1"; font.pixelSize: 12 }
                    SpinBox {
                        id: peopleSpin
                        from: 1; to: 50; value: 4
                        Layout.preferredWidth: 100
                    }

                    Text { text: "Urgency:"; color: "#CBD5E1"; font.pixelSize: 12 }
                    ComboBox {
                        id: urgencyCombo
                        model: ["Minor", "Delayed", "Immediate", "Critical SOS"]
                        currentIndex: 3
                        Layout.fillWidth: true
                    }
                }

                Button {
                    id: meshBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    background: Rectangle {
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: meshBtn.down ? "#7C2D12" : "#EA580C" }
                            GradientStop { position: 1.0; color: "#C2410C" }
                        }
                    }
                    contentItem: Text {
                        text: "📡  Encode & Authenticate Mesh Packet"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        prototypeBackend.runMeshPacketBenchmark(urgencyCombo.currentIndex, 88, peopleSpin.value);
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0B0F19"
                    border.color: "#1F2937"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "BINARY PAYLOAD (HEX):"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                width: 50; height: 18; radius: 4
                                color: root.crcValid ? "#064E3B" : "#7F1D1D"
                                Text {
                                    anchors.centerIn: parent
                                    text: root.crcValid ? "CRC OK" : "CRC ERR"
                                    color: root.crcValid ? "#6EE7B7" : "#FCA5A5"
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                }
                            }
                        }

                        Text {
                            text: root.packetHexStr
                            color: "#A7F3D0"
                            font.pixelSize: 11
                            font.family: "monospace"
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }

                        Text { text: "HMAC-SHA256 SIGNATURE:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text {
                            text: root.hmacSigStr
                            color: "#FBBF24"
                            font.pixelSize: 10
                            font.family: "monospace"
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Decoded: " + root.meshDecodedStr
                            color: "#94A3B8"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Tab 2: MCI Disaster Triage & Survival Analytics
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 2
            radius: 12
            color: "#111827"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "Mass Casualty Triage & Survival Model"
                    color: "#F9FAFB"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                // Triage Vitals Checkboxes
                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 4
                    columnSpacing: 10

                    CheckBox {
                        id: walkCheck; text: "Can Walk (Minor)"; checked: false
                        contentItem: Text { text: walkCheck.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 }
                    }
                    CheckBox {
                        id: breathCheck; text: "Breathing Spontaneously"; checked: true
                        contentItem: Text { text: breathCheck.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 }
                    }
                    CheckBox {
                        id: pulseCheck; text: "Radial Pulse Present"; checked: true
                        contentItem: Text { text: pulseCheck.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 }
                    }
                    CheckBox {
                        id: bleedCheck; text: "Severe Hemorrhage"; checked: true
                        contentItem: Text { text: bleedCheck.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 }
                    }
                    CheckBox {
                        id: trappedCheck; text: "Trapped in Rubble"; checked: true
                        contentItem: Text { text: trappedCheck.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 }
                    }
                }

                Button {
                    id: triageBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    background: Rectangle {
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: triageBtn.down ? "#991B1B" : "#DC2626" }
                            GradientStop { position: 1.0; color: "#B91C1C" }
                        }
                    }
                    contentItem: Text {
                        text: "🚑  Evaluate START Triage & Survival"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        prototypeBackend.runTriageAssessment(
                            walkCheck.checked, breathCheck.checked, 28,
                            pulseCheck.checked, bleedCheck.checked, trappedCheck.checked
                        );
                    }
                }

                // Triage Result Tag
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0B0F19"
                    border.color: "#1F2937"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Rectangle {
                                width: 100; height: 26; radius: 6
                                color: root.triageColor
                                Text {
                                    anchors.centerIn: parent
                                    text: root.triageCategory
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "Urgency: " + root.triageScore + "/100"
                                color: "#F87171"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }
                        }

                        Text {
                            text: "Reason: " + root.triageReason
                            color: "#E2E8F0"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Action: " + root.triageAction
                            color: "#38BDF8"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Hayward Core Survival Probability:"
                                color: "#94A3B8"
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: root.survivalPercent + "%"
                                color: root.survivalPercent > 50 ? "#34D399" : "#F87171"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Tab 3: Spatial KD-Tree Shelters & Sensor Fusion
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 3
            radius: 12
            color: "#111827"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text {
                    text: "Spatial KD-Tree & Kalman Sensor Fusion"
                    color: "#F9FAFB"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: shelterBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        background: Rectangle {
                            radius: 8
                            color: shelterBtn.down ? "#047857" : "#059669"
                        }
                        contentItem: Text {
                            text: "Find Nearest Shelter"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: prototypeBackend.runSpatialShelterQuery(30.3165, 78.0322)
                    }

                    Button {
                        id: kalmanBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        background: Rectangle {
                            radius: 8
                            color: kalmanBtn.down ? "#4338CA" : "#4F46E5"
                        }
                        contentItem: Text {
                            text: "Step EKF Filter"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: prototypeBackend.runKalmanFusionStep(30.3165, 78.0322, 0.4, 0.1)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0B0F19"
                    border.color: "#1F2937"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text { text: "NEAREST SAFE HAVEN (KD-TREE):"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text {
                            text: root.nearestShelter + " (" + root.shelterDistKm + " km)"
                            color: "#34D399"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }

                        Text { text: "EXTENDED KALMAN FILTER FUSED POSITION:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text {
                            text: root.fusedLocationStr
                            color: "#38BDF8"
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                        Text {
                            text: "Fused Speed: " + root.fusedSpeed + " m/s"
                            color: "#94A3B8"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Tab 4: Live Event Console
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 4
            radius: 12
            color: "#0B0F19"
            border.color: "#1F2937"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "C++ Engine Event Stream"
                        color: "#9CA3AF"
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Events: " + prototypeBackend.triggerCount
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
