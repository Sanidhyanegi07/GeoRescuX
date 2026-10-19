import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 480
    height: 860
    minimumWidth: 380
    minimumHeight: 680
    title: qsTr("GeoRescuX — Native C++20 Disaster Response Engine")
    color: "#080C14" // Deep midnight obsidian

    property int activeTab: 0

    // Routing & Map State
    property string routePathStr: "DED_ISBT ➔ DED_CLOCK_TWR ➔ JOLLY_GRANT_HOSP ➔ RISHIKESH_CAMP"
    property string routeStatsStr: "Tap 'Calculate A* Evacuation Route' to compute corridor."
    property string routeWarningsStr: "None (Awaiting analysis)"
    property var activeWaypoints: []
    property var activeShelters: []
    property var activeHazard: ({})
    property var routePointsForMap: [
        { lat: 30.2858, lon: 78.0098, id: "DED_ISBT" },
        { lat: 30.3255, lon: 78.0436, id: "CLOCK_TWR" },
        { lat: 30.3789, lon: 78.0772, id: "MALSI_SHELTER" },
        { lat: 30.1878, lon: 78.1884, id: "JOLLY_GRANT" },
        { lat: 30.1033, lon: 78.2947, id: "RISHIKESH" },
        { lat: 29.9457, lon: 78.1642, id: "HARIDWAR" }
    ]

    // Mesh State
    property string packetHexStr: "013f00018f4a21e0ffffffff6714... (24 bytes)"
    property string hmacSigStr: "Awaiting generation..."
    property string meshDecodedStr: "No active packet"
    property bool crcValid: true
    property string epidemicRelayStr: "DTN buffer standby"

    // Triage State
    property string triageCategory: "IMMEDIATE"
    property string triageColor: "#EF4444"
    property int triageScore: 92
    property string triageReason: "Uncontrolled hemorrhage + trapped in collapse"
    property string triageAction: "Apply high-pressure tourniquet, structural shoring"
    property real survivalPercent: 74.5

    // Spatial & EKF State
    property string nearestShelter: "Jolly Grant Trauma Safe Haven"
    property real shelterDistKm: 18.4
    property string fusedLocationStr: "30.31650° N, 78.03220° E (±2.5m)"
    property real fusedSpeed: 4.8

    // Acoustic Resonance State
    property string acousticPeaksStr: "2950 Hz (-14.2 dB)"
    property bool isRhythmicTapping: true
    property bool isMorseCode: true
    property real regularityScore: 0.94

    Component.onCompleted: {
        root.activeWaypoints = prototypeBackend.getMapWaypoints();
        root.activeShelters = prototypeBackend.getMapShelters();
        root.activeHazard = prototypeBackend.getActiveHazardZone();
        mapCanvas.requestPaint();
    }

    Connections {
        target: prototypeBackend

        function onRouteCalculated(nodePath, distanceMeters, costMeters, durationSeconds, warnings, stepInstructions) {
            root.routePathStr = nodePath;
            const distKm = (distanceMeters / 1000.0).toFixed(2);
            const costKm = (costMeters / 1000.0).toFixed(2);
            const mins = Math.round(durationSeconds / 60);
            root.routeStatsStr = "Distance: " + distKm + " km | Hazard Cost: " + costKm + " km | ETA: " + mins + " min";
            root.routeWarningsStr = warnings;

            navigationModel.clear();
            for (let i = 0; i < stepInstructions.length; ++i) {
                navigationModel.append(stepInstructions[i]);
            }
            mapCanvas.requestPaint();
        }

        function onMeshPacketProcessed(packetHex, sizeBytes, crcOk, hmacDigest, decodedSummary) {
            root.packetHexStr = packetHex;
            root.crcValid = crcOk;
            root.hmacSigStr = hmacDigest;
            root.meshDecodedStr = decodedSummary;
        }

        function onMeshRelayUpdated(queueSize, forwardCount, bloomFiltersCount, relayReport) {
            root.epidemicRelayStr = relayReport;
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
            mapCanvas.requestPaint();
        }

        function onLocationFused(lat, lon, speedMps, accuracyMeters) {
            root.fusedLocationStr = lat.toFixed(5) + "° N, " + lon.toFixed(5) + "° E (±" + accuracyMeters.toFixed(1) + "m)";
            root.fusedSpeed = speedMps.toFixed(1);
            mapCanvas.requestPaint();
        }

        function onAcousticPatternDetected(isRhythmic, isMorse, periodMs, regScore, peaksSummary) {
            root.isRhythmicTapping = isRhythmic;
            root.isMorseCode = isMorse;
            root.regularityScore = regScore;
            root.acousticPeaksStr = peaksSummary;
        }

        function onLogMessageEmitted(formattedMessage) {
            logModel.append({ message: formattedMessage });
            if (logListView.count > 0) {
                logListView.positionViewAtEnd();
            }
        }
    }

    ListModel { id: navigationModel }
    ListModel { id: logModel }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // =====================================================================
        // Header Bar with Mission Health Indicator
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
                        text: "Native C++20 Search & Rescue Hub"
                        color: "#38BDF8"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                Rectangle {
                    width: 96
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
        // Subsystem Tab Navigation
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Repeater {
                model: [
                    { name: "Map & Route", icon: "🗺️" },
                    { name: "Mesh SOS", icon: "📡" },
                    { name: "Triage", icon: "🚑" },
                    { name: "Acoustic", icon: "🎙️" },
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
                        Text { text: modelData.icon; font.pixelSize: 12 }
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
        // Tab 0: Visual Evacuation Map Canvas & A* Routing
        // =====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 0
            spacing: 8

            // 2D Visual Map Canvas
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                radius: 12
                color: "#0D131F"
                border.color: "#1E293B"
                border.width: 1
                clip: true

                Canvas {
                    id: mapCanvas
                    anchors.fill: parent
                    anchors.margins: 6

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();

                        var w = width;
                        var h = height;

                        // Bounds: lat [29.90, 30.42], lon [77.95, 78.35]
                        var minLat = 29.90, maxLat = 30.42;
                        var minLon = 77.95, maxLon = 78.35;

                        function toX(lon) { return ((lon - minLon) / (maxLon - minLon)) * w; }
                        function toY(lat) { return h - (((lat - minLat) / (maxLat - minLat)) * h); }

                        // Draw Grid Lines
                        ctx.strokeStyle = "#162032";
                        ctx.lineWidth = 1;
                        for (var i = 1; i <= 4; ++i) {
                            ctx.beginPath();
                            ctx.moveTo(0, (h / 5) * i);
                            ctx.lineTo(w, (h / 5) * i);
                            ctx.stroke();

                            ctx.beginPath();
                            ctx.moveTo((w / 5) * i, 0);
                            ctx.lineTo((w / 5) * i, h);
                            ctx.stroke();
                        }

                        // Draw Hazard Exclusion Zone (Flood/Landslide)
                        var hazX = toX(30.25 ? 78.12 : 78.12);
                        var hazY = toY(30.25);
                        ctx.fillStyle = "rgba(239, 68, 68, 0.20)";
                        ctx.strokeStyle = "#EF4444";
                        ctx.lineWidth = 1.5;
                        ctx.beginPath();
                        ctx.arc(hazX, hazY, 34, 0, 2 * Math.PI);
                        ctx.fill();
                        ctx.stroke();

                        ctx.fillStyle = "#F87171";
                        ctx.font = "bold 9px monospace";
                        ctx.fillText("HAZARD ZONE", hazX - 30, hazY - 38);

                        // Draw Road Network Edges
                        var pts = root.routePointsForMap;
                        ctx.strokeStyle = "#334155";
                        ctx.lineWidth = 2;
                        ctx.setLineDash([4, 4]);
                        ctx.beginPath();
                        ctx.moveTo(toX(pts[0].lon), toY(pts[0].lat));
                        for (var j = 1; j < pts.length; ++j) {
                            ctx.lineTo(toX(pts[j].lon), toY(pts[j].lat));
                        }
                        ctx.stroke();
                        ctx.setLineDash([]);

                        // Draw Evacuation Route (A* Active Path)
                        ctx.strokeStyle = "#38BDF8";
                        ctx.lineWidth = 3.5;
                        ctx.beginPath();
                        ctx.moveTo(toX(pts[0].lon), toY(pts[0].lat)); // Dehradun ISBT
                        ctx.lineTo(toX(pts[1].lon), toY(pts[1].lat)); // Clock Tower
                        ctx.lineTo(toX(pts[2].lon), toY(pts[2].lat)); // Malsi Bypass
                        ctx.lineTo(toX(pts[3].lon), toY(pts[3].lat)); // Jolly Grant
                        ctx.lineTo(toX(pts[4].lon), toY(pts[4].lat)); // Rishikesh
                        ctx.stroke();

                        // Draw Waypoint Pins & Labels
                        for (var k = 0; k < pts.length; ++k) {
                            var px = toX(pts[k].lon);
                            var py = toY(pts[k].lat);

                            ctx.fillStyle = (k === 0 || k === 4) ? "#38BDF8" : "#10B981";
                            ctx.beginPath();
                            ctx.arc(px, py, 5, 0, 2 * Math.PI);
                            ctx.fill();

                            ctx.fillStyle = "#E2E8F0";
                            ctx.font = "10px sans-serif";
                            ctx.fillText(pts[k].id, px + 8, py + 3);
                        }

                        // Draw Real-time GPS Location Pin (Pulsing blue dot)
                        var gpsX = toX(78.0322);
                        var gpsY = toY(30.3165);
                        ctx.fillStyle = "rgba(56, 189, 248, 0.35)";
                        ctx.beginPath();
                        ctx.arc(gpsX, gpsY, 12, 0, 2 * Math.PI);
                        ctx.fill();

                        ctx.fillStyle = "#0284C7";
                        ctx.beginPath();
                        ctx.arc(gpsX, gpsY, 5, 0, 2 * Math.PI);
                        ctx.fill();
                    }
                }

                // Map Legend Overlay
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 8
                    width: 140
                    height: 22
                    radius: 4
                    color: "#D90B0F19"
                    border.color: "#1F2937"
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle { width: 8; height: 3; color: "#38BDF8" }
                        Text { text: "A* Path"; color: "#CBD5E1"; font.pixelSize: 9 }
                        Rectangle { width: 6; height: 6; radius: 3; color: "#EF4444" }
                        Text { text: "Hazard"; color: "#CBD5E1"; font.pixelSize: 9 }
                    }
                }
            }

            // Controls & Instructions
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#111827"
                border.color: "#1F2937"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            id: mapHazardCheck
                            checked: true
                            text: "Avoid Thano Hazard Zone (Flood Bypass)"
                            contentItem: Text {
                                text: mapHazardCheck.text
                                color: "#E2E8F0"
                                font.pixelSize: 11
                                leftPadding: 24
                            }
                        }
                    }

                    Button {
                        id: computeRouteBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        background: Rectangle {
                            radius: 8
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: computeRouteBtn.down ? "#0369A1" : "#0284C7" }
                                GradientStop { position: 1.0; color: "#075985" }
                            }
                        }
                        contentItem: Text {
                            text: "▶  Compute Safe Evacuation Route (A*)"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: prototypeBackend.runAStarRoutingBenchmark(mapHazardCheck.checked)
                    }

                    Text {
                        text: root.routeStatsStr
                        color: "#34D399"
                        font.pixelSize: 11
                        font.family: "monospace"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    // Turn-by-Turn Guidance List
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: navigationModel
                        delegate: Item {
                            width: parent.width
                            height: 24
                            RowLayout {
                                anchors.fill: parent
                                spacing: 8
                                Text { text: "➔"; color: "#38BDF8"; font.pixelSize: 12 }
                                Text {
                                    text: model.instruction
                                    color: "#CBD5E1"
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Tab 1: BLE Mesh SOS & Epidemic DTN Relay
        // =====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 1
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                radius: 12
                color: "#111827"
                border.color: "#1F2937"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text { text: "BLE Mesh SOS Packet Generation"; color: "#F9FAFB"; font.pixelSize: 14; font.weight: Font.Bold }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Victims:"; color: "#CBD5E1"; font.pixelSize: 11 }
                        SpinBox { id: peopleInput; from: 1; to: 50; value: 4; Layout.preferredWidth: 90 }
                        Text { text: "Urgency:"; color: "#CBD5E1"; font.pixelSize: 11 }
                        ComboBox {
                            id: urgInput
                            model: ["Minor", "Delayed", "Immediate", "Critical SOS"]
                            currentIndex: 3
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        id: sendSosBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            radius: 8
                            color: sendSosBtn.down ? "#C2410C" : "#EA580C"
                        }
                        contentItem: Text {
                            text: "📡  Generate & Sign SOS Distress Beacon"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: prototypeBackend.runMeshPacketBenchmark(urgInput.currentIndex, 92, peopleInput.value)
                    }

                    Text {
                        text: "Payload: " + root.packetHexStr
                        color: "#A7F3D0"
                        font.pixelSize: 10
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#111827"
                border.color: "#1F2937"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text { text: "Epidemic DTN Store-Carry-and-Forward Router"; color: "#F9FAFB"; font.pixelSize: 14; font.weight: Font.Bold }
                    Text {
                        text: "Simulates decentralized packet flooding with Bloom Filter duplicate rejection and TTL hop counting."
                        color: "#94A3B8"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Button {
                        id: relayBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            radius: 8
                            color: relayBtn.down ? "#047857" : "#059669"
                        }
                        contentItem: Text {
                            text: "🔄  Simulate Multi-Hop Mesh Gossip Relay"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: prototypeBackend.runEpidemicMeshRelayBenchmark()
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: "#0B0F19"
                        border.color: "#1F2937"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Text { text: "ROUTER STATUS REPORT:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                            Text {
                                text: root.epidemicRelayStr
                                color: "#34D399"
                                font.pixelSize: 11
                                font.family: "monospace"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }
                            Text { text: "ANTI-SPOOF HMAC SIGNATURE:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                            Text {
                                text: root.hmacSigStr
                                color: "#FBBF24"
                                font.pixelSize: 10
                                font.family: "monospace"
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
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

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text { text: "Mass Casualty Triage & Survival Model"; color: "#F9FAFB"; font.pixelSize: 15; font.weight: Font.DemiBold }

                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 4
                    columnSpacing: 10

                    CheckBox { id: tWalk; text: "Can Walk (Minor)"; checked: false; contentItem: Text { text: tWalk.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 } }
                    CheckBox { id: tBreath; text: "Breathing Spontaneously"; checked: true; contentItem: Text { text: tBreath.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 } }
                    CheckBox { id: tPulse; text: "Radial Pulse Present"; checked: true; contentItem: Text { text: tPulse.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 } }
                    CheckBox { id: tBleed; text: "Severe Hemorrhage"; checked: true; contentItem: Text { text: tBleed.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 } }
                    CheckBox { id: tTrap; text: "Trapped in Rubble"; checked: true; contentItem: Text { text: tTrap.text; color: "#CBD5E1"; font.pixelSize: 11; leftPadding: 24 } }
                }

                Button {
                    id: tBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    background: Rectangle {
                        radius: 8
                        color: tBtn.down ? "#991B1B" : "#DC2626"
                    }
                    contentItem: Text {
                        text: "🚑  Evaluate START Triage & Survival Probability"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        prototypeBackend.runTriageAssessment(
                            tWalk.checked, tBreath.checked, 28,
                            tPulse.checked, tBleed.checked, tTrap.checked
                        );
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

                        Text { text: "Reason: " + root.triageReason; color: "#E2E8F0"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Text { text: "Action: " + root.triageAction; color: "#38BDF8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Core Body Survival Probability (Hayward Model):"; color: "#94A3B8"; font.pixelSize: 11 }
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
        // Tab 3: Acoustic Resonance & Whistle / Rubble Tapping Detector
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.activeTab === 3
            radius: 12
            color: "#111827"
            border.color: "#1F2937"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Text { text: "Acoustic Resonance & Rubble Tapping Detector"; color: "#F9FAFB"; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text {
                    text: "Analyzes 16 kHz microphone PCM audio with Cooley-Tukey Radix-2 FFT to detect survivor distress whistles (2.8–3.2 kHz) and rhythmic rubble tapping."
                    color: "#94A3B8"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Button {
                    id: acousticBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    background: Rectangle {
                        radius: 8
                        color: acousticBtn.down ? "#4338CA" : "#4F46E5"
                    }
                    contentItem: Text {
                        text: "🎙️  Run 16 kHz FFT Acoustic Analysis"
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: prototypeBackend.runAcousticDetectionBenchmark(true)
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0B0F19"
                    border.color: "#1F2937"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text { text: "SPECTRAL RESONANCE PEAKS:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                        Text {
                            text: root.acousticPeaksStr
                            color: "#38BDF8"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Rhythmic Tapping:"; color: "#CBD5E1"; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: root.isRhythmicTapping ? "DETECTED" : "NONE"
                                color: root.isRhythmicTapping ? "#34D399" : "#6B7280"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Morse Code SOS (... --- ...):"; color: "#CBD5E1"; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: root.isMorseCode ? "CONFIRMED" : "INACTIVE"
                                color: root.isMorseCode ? "#FBBF24" : "#6B7280"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Pattern Regularity Score:"; color: "#CBD5E1"; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: (root.regularityScore * 100.0).toFixed(0) + " / 100"
                                color: "#38BDF8"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
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

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "C++ Engine Event Stream"; color: "#9CA3AF"; font.pixelSize: 12; font.weight: Font.Bold }
                    Item { Layout.fillWidth: true }
                    Text { text: "Events: " + prototypeBackend.triggerCount; color: "#6B7280"; font.pixelSize: 11 }
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
