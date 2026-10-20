import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GeoRescuX.Prototype 1.0

Rectangle {
    id: prototypeLabScreen
    color: "#080C14"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Text { text: "🔬 C++20 Search & Rescue Engine Lab (Phase 1 Baseline)"; color: "#38BDF8"; font.pixelSize: 14; font.weight: Font.Bold }

        // Visual 2D Map Canvas
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 180; radius: 10; color: "#0D131F"; border.color: "#1E293B"; clip: true
            Canvas {
                id: labCanvas
                anchors.fill: parent; anchors.margins: 6
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    var w = width; var h = height;
                    ctx.strokeStyle = "#38BDF8"; ctx.lineWidth = 3;
                    ctx.beginPath(); ctx.moveTo(w * 0.1, h * 0.8); ctx.lineTo(w * 0.3, h * 0.6);
                    ctx.lineTo(w * 0.5, h * 0.7); ctx.lineTo(w * 0.8, h * 0.3); ctx.stroke();
                    ctx.fillStyle = "#EF4444"; ctx.beginPath(); ctx.arc(w * 0.5, h * 0.3, 18, 0, 2*Math.PI); ctx.fill();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                background: Rectangle { radius: 6; color: "#0284C7" }
                contentItem: Text { text: "A* Pathfinding"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: prototypeBackend.runAStarRoutingBenchmark(true)
            }
            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                background: Rectangle { radius: 6; color: "#EA580C" }
                contentItem: Text { text: "BLE Mesh Packet"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: prototypeBackend.runMeshPacketBenchmark(3, 85, 4)
            }
            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                background: Rectangle { radius: 6; color: "#DC2626" }
                contentItem: Text { text: "START Triage"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: prototypeBackend.runTriageAssessment(false, true, 28, true, true, true)
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; radius: 8; color: "#0B0F19"; border.color: "#1E293B"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 4
                Text { text: "ENGINE STATUS FEED:"; color: "#64748B"; font.pixelSize: 9; font.weight: Font.Bold }
                Text { text: prototypeBackend.statusText; color: "#34D399"; font.pixelSize: 11; font.family: "monospace"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }
                Text { text: "ROUTE SUMMARY:"; color: "#64748B"; font.pixelSize: 9; font.weight: Font.Bold }
                Text { text: prototypeBackend.routeSummary; color: "#38BDF8"; font.pixelSize: 10; font.family: "monospace"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
    }
}
