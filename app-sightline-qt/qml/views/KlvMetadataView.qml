import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

ScrollView {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: Math.max(availableWidth, minContentWidth + 32)
    contentHeight: mainCol.implicitHeight + 32
    clip: true
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

    readonly property int minContentWidth: 720

    // Selected Display Output for KLV metadata injection (2 = Net0, 7 = Net1, 9 = Net2)
    property int selectedDisplayId: displayIdCombo.currentIndex === 0 ? 2 : (displayIdCombo.currentIndex === 1 ? 7 : 9)
    property int activeTab: 0

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Header Title Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle { width: 4; height: 22; color: SightlineTheme.primary; radius: 2 }

            ColumnLayout {
                spacing: 2
                RowLayout {
                    spacing: 8
                    Text {
                        text: "MISB ST 0601 / ST 0102 / ST 0903 / STANAG 4609 KLV ENGINE"
                        color: SightlineTheme.textPrimary
                        font.pixelSize: SightlineTheme.fontSizeMedium
                        font.bold: true
                        font.letterSpacing: 1.0
                    }
                    Rectangle {
                        implicitWidth: 84
                        implicitHeight: 18
                        radius: 4
                        color: Qt.rgba(0.0, 0.9, 1.0, 0.15)
                        border.color: SightlineTheme.primary
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "IDD 3.11 / STANAG"
                            color: SightlineTheme.primary
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }
                Text {
                    text: "// Telemetry Injection, Static Security, OLS DTED Terrain, VMTI, CoT & ST 0808 Text"
                    color: SightlineTheme.textMuted
                    font.pixelSize: SightlineTheme.fontSizeSmall
                    font.family: "Monospace"
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Target Stream:"
                color: SightlineTheme.textSecondary
                font.pixelSize: 11
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            ComboBox {
                id: displayIdCombo
                model: [
                    "Net0 Primary (Display ID 2, :15004)",
                    "Net1 Secondary (Display ID 7, :15104)",
                    "Net2 Tertiary (Display ID 9, :15204)"
                ]
                Layout.preferredWidth: 260
                Layout.preferredHeight: 30
            }
        }

        // Live Telemetry Metric Cards Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10

            MetricCard {
                title: "Aircraft Position"
                value: parseFloat(latInput.text).toFixed(4) + "°, " + parseFloat(lonInput.text).toFixed(4) + "°"
                unit: ""
                accentColor: SightlineTheme.primary
                iconText: "🌐"
            }
            MetricCard {
                title: "Altitude MSL"
                value: altInput.text
                unit: "m"
                accentColor: SightlineTheme.info
                iconText: "⛰️"
            }
            MetricCard {
                title: "Attitude (H/P/R)"
                value: headingInput.text + "° / " + pitchInput.text + "° / " + rollInput.text + "°"
                unit: ""
                accentColor: SightlineTheme.warning
                iconText: "🧭"
            }
            MetricCard {
                title: "Sensor Optics"
                value: "Az " + azInput.text + "° | El " + elInput.text + "°"
                unit: "HFOV " + hfovInput.text + "°"
                accentColor: SightlineTheme.success
                iconText: "🔭"
            }
            MetricCard {
                title: "Target Slant Range"
                value: slantRangeInput.text
                unit: "m"
                accentColor: SightlineTheme.accent
                iconText: "🎯"
            }
            MetricCard {
                title: "Master KLV Rate"
                value: klvFrameStepCombo.currentText.split(" ")[0]
                unit: "Hz"
                accentColor: SightlineTheme.primary
                iconText: "⏱️"
            }
        }

        // Sub-Navigation Tab Selector Row
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: tabRow.implicitHeight + 8
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            RowLayout {
                id: tabRow
                anchors.fill: parent
                anchors.margins: 4
                spacing: 6

                Repeater {
                    model: [
                        { label: "✈️ Platform & Sensor", sub: "0x13 / 0x8B" },
                        { label: "🛡️ Mission & Security", sub: "0x14 ST 0102" },
                        { label: "🎯 Ground & Terrain", sub: "0x15 OLS DTED" },
                        { label: "⏱️ Rates & Sources", sub: "0x62, 0x97, 0x98" },
                        { label: "📡 CoT & Ancillary Text", sub: "0xB0, 0xAC" },
                        { label: "🤖 VMTI & Image Chips", sub: "0x84, 0xBF, 0xAD" }
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: SightlineTheme.radiusSmall
                        color: root.activeTab === index ? Qt.rgba(0.0, 0.9, 1.0, 0.15) : (tabMouse.containsMouse ? SightlineTheme.surfaceLight : "transparent")
                        border.color: root.activeTab === index ? SightlineTheme.primary : "transparent"
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                text: modelData.label
                                color: root.activeTab === index ? SightlineTheme.primary : (tabMouse.containsMouse ? SightlineTheme.textPrimary : SightlineTheme.textSecondary)
                                font.pixelSize: 11
                                font.bold: root.activeTab === index
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: modelData.sub
                                color: root.activeTab === index ? SightlineTheme.primaryHover : SightlineTheme.textMuted
                                font.pixelSize: 9
                                font.family: "Monospace"
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeTab = index
                        }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 0: PLATFORM & SENSOR ATTITUDE (0x13 / 0x8B)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: tab0Col.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: tab0Col
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "UAS GEODETIC POSITION & SENSOR ATTITUDE (MESSAGE ID 0x13)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0601 Tags 5-7, 13-17, 19-20"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Injects aircraft GPS geodetic coordinates, platform attitude (yaw/pitch/roll), and sensor gimbal pointing angles into the active STANAG 4609 KLV metadata stream."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    // Aircraft Position
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Geodetic Position:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.alignment: Qt.AlignVCenter }

                        Text { text: "Lat (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: latInput; text: "37.7749"; Layout.preferredWidth: 100; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: latInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Lon (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: lonInput; text: "-122.4194"; Layout.preferredWidth: 105; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: lonInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Alt MSL (m):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: altInput; text: "1500.0"; Layout.preferredWidth: 85; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: altInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Platform Attitude
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Platform Attitude:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.alignment: Qt.AlignVCenter }

                        Text { text: "Heading (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: headingInput; text: "180.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: headingInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Pitch (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: pitchInput; text: "-5.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: pitchInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Roll (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: rollInput; text: "0.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: rollInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Sensor Optics & Pointing
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Sensor Pointing / FOV:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.alignment: Qt.AlignVCenter }

                        Text { text: "HFOV (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: hfovInput; text: "30.0"; Layout.preferredWidth: 70; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: hfovInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "VFOV (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 4 }
                        TextField {
                            id: vfovInput; text: "20.0"; Layout.preferredWidth: 70; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: vfovInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Azimuth (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 4 }
                        TextField {
                            id: azInput; text: "0.0"; Layout.preferredWidth: 70; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: azInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Elevation (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 4 }
                        TextField {
                            id: elInput; text: "-15.0"; Layout.preferredWidth: 70; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: elInput.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Presets
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Layout.topMargin: 4

                        Text { text: "Quick Flight Presets:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.preferredWidth: 140 }

                        Button {
                            text: "Level Cruise (1500m)"
                            Layout.preferredHeight: 26
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                altInput.text = "1500.0";
                                headingInput.text = "0.0";
                                pitchInput.text = "0.0";
                                rollInput.text = "0.0";
                                azInput.text = "0.0";
                                elInput.text = "-10.0";
                            }
                        }

                        Button {
                            text: "30° Right Bank & Orbit"
                            Layout.preferredHeight: 26
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                rollInput.text = "30.0";
                                pitchInput.text = "2.0";
                                azInput.text = "90.0";
                                elInput.text = "-25.0";
                            }
                        }

                        Button {
                            text: "High-Alt Recon (5000m)"
                            Layout.preferredHeight: 26
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                altInput.text = "5000.0";
                                hfovInput.text = "5.0";
                                vfovInput.text = "3.5";
                                elInput.text = "-45.0";
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Actions Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Layout.topMargin: 8

                        Button {
                            text: "🚀 Transmit Single Frame (0x13)"
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 32
                            background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                            contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadata(
                                        parseFloat(latInput.text),
                                        parseFloat(lonInput.text),
                                        parseFloat(altInput.text),
                                        parseFloat(headingInput.text),
                                        parseFloat(pitchInput.text),
                                        parseFloat(rollInput.text),
                                        parseFloat(hfovInput.text),
                                        parseFloat(vfovInput.text),
                                        parseFloat(azInput.text),
                                        parseFloat(elInput.text),
                                        root.selectedDisplayId
                                    );
                                }
                            }
                        }

                        CheckBox {
                            id: autoStreamKlv
                            text: "Simulate 10 Hz Flight Telemetry & Dead-Reckoning"
                            checked: false
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Timer {
                        id: klvSimTimer
                        interval: 100
                        running: autoStreamKlv.checked
                        repeat: true
                        onTriggered: {
                            var h = (parseFloat(headingInput.text) + 0.2) % 360.0;
                            headingInput.text = h.toFixed(1);

                            // Advance position by ~0.00001 deg (~1.1 meter per tick at ~40 knots)
                            var rad = h * Math.PI / 180.0;
                            var newLat = parseFloat(latInput.text) + Math.cos(rad) * 0.00001;
                            var newLon = parseFloat(lonInput.text) + Math.sin(rad) * 0.00001;
                            latInput.text = newLat.toFixed(5);
                            lonInput.text = newLon.toFixed(5);

                            if (bridge) {
                                bridge.setMetadata(
                                    newLat,
                                    newLon,
                                    parseFloat(altInput.text),
                                    h,
                                    parseFloat(pitchInput.text),
                                    parseFloat(rollInput.text),
                                    parseFloat(hfovInput.text),
                                    parseFloat(vfovInput.text),
                                    parseFloat(azInput.text),
                                    parseFloat(elInput.text),
                                    root.selectedDisplayId
                                );
                            }
                        }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 1: MISSION & SECURITY CLASSIFICATION (0x14 - ST 0102)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 1

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: tab1Col.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: tab1Col
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "STATIC MISSION IDENTIFIERS & MISB ST 0102 SECURITY CLASSIFICATION (0x14)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0102 / ST 0601 Tags 1, 3, 4, 6, 11"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Static values are transmitted periodically in the KLV stream (or at mission startup) to establish security markings, callsign, and mission tracking."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    // Tag 1: Classification
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Security Classification (Tag 1):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        ComboBox {
                            id: secClassCombo
                            model: [
                                "1: UNCLASSIFIED (//UNCLASS)",
                                "2: RESTRICTED (//RESTRICTED)",
                                "3: CONFIDENTIAL (//CONFIDENTIAL)",
                                "4: SECRET (//SECRET)",
                                "5: TOP SECRET (//TOP SECRET)"
                            ]
                            currentIndex: 0
                            Layout.preferredWidth: 280
                            Layout.preferredHeight: 30
                        }

                        Button {
                            text: "Push Tag 1"
                            Layout.preferredHeight: 28
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(1, (secClassCombo.currentIndex + 1).toString(), root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Tag 3: Mission ID
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Mission ID (Tag 3):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        TextField {
                            id: missionIdField
                            text: "PATROL-ALPHA-01"
                            Layout.preferredWidth: 280
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Button {
                            text: "Push Tag 3"
                            Layout.preferredHeight: 28
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(3, missionIdField.text, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Tag 4: Platform Tail Number
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Platform Tail Number (Tag 4):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        TextField {
                            id: tailNumberField
                            text: "UAV-904X"
                            Layout.preferredWidth: 280
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Button {
                            text: "Push Tag 4"
                            Layout.preferredHeight: 28
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(4, tailNumberField.text, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Tag 6: Country Coding / Releasing Instructions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Releasing Instructions (Tag 6):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        TextField {
                            id: releasingField
                            text: "USA//REL TO USA, NATO"
                            Layout.preferredWidth: 280
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Button {
                            text: "Push Tag 6"
                            Layout.preferredHeight: 28
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(6, releasingField.text, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Tag 11: Image Source Sensor
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Image Source Sensor (Tag 11):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        TextField {
                            id: sensorSourceField
                            text: "EO-DAYLIGHT-4K"
                            Layout.preferredWidth: 280
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Button {
                            text: "Push Tag 11"
                            Layout.preferredHeight: 28
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(11, sensorSourceField.text, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Batch Push All Button
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8

                        Button {
                            text: "🛡️ Broadcast All Static & Security Metadata (0x14)"
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: 320
                            background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                            contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataStatic(1, (secClassCombo.currentIndex + 1).toString(), root.selectedDisplayId);
                                    bridge.setMetadataStatic(3, missionIdField.text, root.selectedDisplayId);
                                    bridge.setMetadataStatic(4, tailNumberField.text, root.selectedDisplayId);
                                    bridge.setMetadataStatic(6, releasingField.text, root.selectedDisplayId);
                                    bridge.setMetadataStatic(11, sensorSourceField.text, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 2: GROUND PROJECTIONS & TERRAIN ELEVATION (0x15 - ST 0601 / OLS DTED)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 2

            // Tactical Banner for OLS DTED Mode
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: dtedBannerCol.implicitHeight + 16
                color: Qt.rgba(0.0, 0.9, 1.0, 0.08)
                radius: SightlineTheme.radiusMedium
                border.color: Qt.rgba(0.0, 0.9, 1.0, 0.3)
                border.width: 1

                ColumnLayout {
                    id: dtedBannerCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    RowLayout {
                        spacing: 8
                        Text { text: "⛰️"; font.pixelSize: 16 }
                        Text {
                            text: "ON-BOARD DTED TERRAIN INTERSECTION (OLS MODE - BIT 5)"
                            color: SightlineTheme.primary
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    Text {
                        text: "Per EAN-KLV-Metadata Section 3.2, enabling Bit 5 activates On-board DTED level 0/1 terrain elevation matrices. The Sightline video processor dynamically computes exact target ground intersections taking real-world topography into account rather than assuming flat earth."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: tab2Col.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: tab2Col
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "FRAME CENTER & GROUND PROJECTION GEOMETRY (MESSAGE ID 0x15)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0601 Tags 21-25"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    // Coordinates
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Frame Center:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }

                        Text { text: "Lat (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: centerLatInput; text: "37.7800"; Layout.preferredWidth: 100; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Lon (°):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: centerLonInput; text: "-122.4100"; Layout.preferredWidth: 105; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "El MSL (m):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: centerElInput; text: "25.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Ground Geometry
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Ground Geometry:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }

                        Text { text: "Frame Width (m):"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: frameWidthInput; text: "120.0"; Layout.preferredWidth: 80; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Slant Range (m):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 12 }
                        TextField {
                            id: slantRangeInput; text: "2500.0"; Layout.preferredWidth: 85; color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Terrain Mode Checkbox
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Layout.topMargin: 4

                        CheckBox {
                            id: dtedEnableCheck
                            text: "Enable On-board DTED Terrain Mode (Bit 5 / 0x20 in flags)"
                            checked: true
                        }
                    }

                    // Actions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Layout.topMargin: 6

                        Button {
                            text: "🎯 Transmit Frame Center & Projections (0x15)"
                            Layout.preferredWidth: 280
                            Layout.preferredHeight: 32
                            background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                            contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    bridge.setMetadataFrame(
                                        parseFloat(centerLatInput.text),
                                        parseFloat(centerLonInput.text),
                                        parseFloat(centerElInput.text),
                                        parseFloat(frameWidthInput.text),
                                        parseFloat(slantRangeInput.text),
                                        dtedEnableCheck.checked,
                                        root.selectedDisplayId
                                    );
                                }
                            }
                        }

                        Button {
                            text: "📐 Calculate from Platform Vector"
                            Layout.preferredHeight: 32
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                var heading = parseFloat(headingInput.text);
                                var az = parseFloat(azInput.text);
                                var totalAzRad = (heading + az) * Math.PI / 180.0;
                                var slant = parseFloat(slantRangeInput.text);
                                var groundDist = slant * Math.cos(parseFloat(elInput.text) * Math.PI / 180.0);

                                var dLat = (groundDist * Math.cos(totalAzRad)) / 111320.0;
                                var dLon = (groundDist * Math.sin(totalAzRad)) / (111320.0 * Math.cos(parseFloat(latInput.text) * Math.PI / 180.0));

                                centerLatInput.text = (parseFloat(latInput.text) + dLat).toFixed(5);
                                centerLonInput.text = (parseFloat(lonInput.text) + dLon).toFixed(5);
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 3: TAG RATES & SOURCE MULTIPLEXING (0x62, 0x97, 0x98)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 3

            // Card 1: Master Rate & Local Sets (0x62)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: rateCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: rateCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "MASTER KLV METADATA RATE & LOCAL SET ENABLES (0x62)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "Message ID 0x62 (SLASetMetadataRate)"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Master Decimation Frame Step:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        ComboBox {
                            id: klvFrameStepCombo
                            model: [
                                "30 Hz (Step 1 - Every Video Frame)",
                                "15 Hz (Step 2 - Every 2nd Frame)",
                                "10 Hz (Step 3 - Every 3rd Frame)",
                                "6 Hz (Step 5 - Every 5th Frame)",
                                "3 Hz (Step 10 - Every 10th Frame)",
                                "1 Hz (Step 30 - Every 30th Frame)"
                            ]
                            currentIndex: 0
                            Layout.preferredWidth: 260
                            Layout.preferredHeight: 30
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Local Sets Enables
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Text { text: "Enabled Local Sets:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

                        CheckBox { id: enableSecCheck; text: "Security (ST 0102)"; checked: true }
                        CheckBox { id: enableUasCheck; text: "UAS Core (ST 0601)"; checked: true }
                        CheckBox { id: enableVmtiCheck; text: "VMTI (ST 0903)"; checked: true }
                        CheckBox { id: enableMiiCheck; text: "MII (ST 1204)"; checked: true }
                        Item { Layout.fillWidth: true }
                    }

                    Button {
                        text: "⏱️ Apply Master KLV Rate & Enables (0x62)"
                        Layout.preferredWidth: 280
                        Layout.preferredHeight: 32
                        background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            if (bridge) {
                                var step = 1;
                                if (klvFrameStepCombo.currentIndex === 1) step = 2;
                                else if (klvFrameStepCombo.currentIndex === 2) step = 3;
                                else if (klvFrameStepCombo.currentIndex === 3) step = 5;
                                else if (klvFrameStepCombo.currentIndex === 4) step = 10;
                                else if (klvFrameStepCombo.currentIndex === 5) step = 30;

                                var mask = 0;
                                if (enableSecCheck.checked) mask |= 0x01;
                                if (enableUasCheck.checked) mask |= 0x02;
                                if (enableVmtiCheck.checked) mask |= 0x04;
                                if (enableMiiCheck.checked) mask |= 0x08;

                                bridge.setMetadataRate(mask, step, root.selectedDisplayId);
                            }
                        }
                    }
                }
            }

            // Card 2: Tag Decimation Rate (0x97) & Tag Source Selector (0x98)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: tagCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: tagCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "INDIVIDUAL TAG DECIMATION & MULTIPLEXER ROUTING (0x97 / 0x98)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "Messages 0x97 (Rate) & 0x98 (Source Selector)"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    // Tag Decimation (0x97)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Tag Decimation Rate (0x97):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 180 }

                        Text { text: "Tag:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        ComboBox {
                            id: decimateTagCombo
                            model: [
                                "Tag 2: Timestamp (Microseconds)",
                                "Tag 5: Platform Heading",
                                "Tag 6: Platform Pitch",
                                "Tag 7: Platform Roll",
                                "Tag 13: Sensor Latitude",
                                "Tag 14: Sensor Longitude",
                                "Tag 15: Sensor Altitude",
                                "Tag 65: VMTI Local Set",
                                "Tag 74: VObject Series"
                            ]
                            currentIndex: 0
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 30
                        }

                        Text { text: "Step:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 6 }
                        SpinBox {
                            id: tagStepSpin
                            from: 0; to: 120; value: 1
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 30
                        }

                        Button {
                            text: "Apply Decimation"
                            Layout.preferredHeight: 30
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    var tagIds = [2, 5, 6, 7, 13, 14, 15, 65, 74];
                                    bridge.setTagDataRate(tagIds[decimateTagCombo.currentIndex], tagStepSpin.value, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Tag Source Selector (0x98)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Tag Source Multiplexer (0x98):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 180 }

                        Text { text: "Tag:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        ComboBox {
                            id: muxTagCombo
                            model: [
                                "Tag 13: Sensor Latitude",
                                "Tag 14: Sensor Longitude",
                                "Tag 15: Sensor Altitude",
                                "Tag 21: Frame Center Lat",
                                "Tag 22: Frame Center Lon"
                            ]
                            currentIndex: 0
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 30
                        }

                        Text { text: "Source:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 6 }
                        ComboBox {
                            id: muxSourceCombo
                            model: [
                                "0: SVP Serial / Network Command",
                                "1: External NMEA / GPS Port",
                                "2: Sightline Vision Tracker Engine",
                                "3: Appended User Blob Passthrough"
                            ]
                            currentIndex: 0
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 30
                        }

                        Button {
                            text: "Apply Multiplexer"
                            Layout.preferredHeight: 30
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: SightlineTheme.inputBorder }
                            contentItem: Text { text: parent.text; color: SightlineTheme.primary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                if (bridge) {
                                    var tagMap = [13, 14, 15, 21, 22];
                                    bridge.setTagSourceSelector(tagMap[muxTagCombo.currentIndex], muxSourceCombo.currentIndex, root.selectedDisplayId);
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 4: TACTICAL CURSOR-ON-TARGET (0xB0) & ST 0808 ANCILLARY TEXT (0xAC)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 4

            // CoT Card (0xB0)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cotCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: cotCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "TACTICAL CURSOR-ON-TARGET (CoT) XML TELEMETRY (MESSAGE ID 0xB0)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0805 / ATAK / WinTAK Interoperability"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Streams MIL-STD Cursor-on-Target XML point telemetry over UDP directly to TAK servers (ATAK, WinTAK) or tactical ground stations."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Telemetry CoT Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                        ComboBox {
                            id: cotModeCombo
                            model: [
                                "0: Disabled / Inactive",
                                "1: Primary Target Track (a-f-G-U-C)",
                                "2: Platform Geodetic Position (a-f-A-M-F-Q)",
                                "3: Sensor Point of Interest (SPI)"
                            ]
                            currentIndex: 1
                            Layout.preferredWidth: 280
                            Layout.preferredHeight: 30
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Target Network Destination:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }

                        Text { text: "IPv4:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        TextField {
                            id: cotIpField
                            text: "239.255.0.1"
                            Layout.preferredWidth: 140
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "UDP Port:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: cotPortField
                            text: "1870"
                            Layout.preferredWidth: 70
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Rate (Hz):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        SpinBox {
                            id: cotRateSpin
                            from: 1; to: 30; value: 2
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 30
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Button {
                        text: "📡 Apply Cursor-on-Target (0xB0)"
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 32
                        Layout.topMargin: 4
                        background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            if (bridge) {
                                bridge.setCursorOnTarget(
                                    cotModeCombo.currentIndex,
                                    cotIpField.text,
                                    parseInt(cotPortField.text) || 1870,
                                    cotRateSpin.value,
                                    root.selectedDisplayId
                                );
                            }
                        }
                    }
                }
            }

            // Ancillary Text Card (0xAC)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: textCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: textCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "MISB ST 0808 DYNAMIC ANCILLARY TEXT INJECTION (MESSAGE ID 0xAC)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0808 / Synchronous MPEG-TS Elementary Stream"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Injects dynamic human-readable UTF-8 mission notifications, tactical alerts, and operator messages directly into the KLV data stream without altering video frames."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Source (max 8 ch):"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
                        TextField {
                            id: textSourceField
                            text: "GCS-01"
                            Layout.preferredWidth: 100
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }

                        Text { text: "Originator (max 16 ch):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        TextField {
                            id: textOrigField
                            text: "OPERATOR"
                            Layout.preferredWidth: 140
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Message Body:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140 }
                        TextField {
                            id: textBodyField
                            text: "TARGET HOTEL ACQUIRED AND TRACKING ACTIVE"
                            Layout.fillWidth: true
                            color: SightlineTheme.textPrimary
                            background: Rectangle { color: SightlineTheme.surfaceLight; radius: 4; border.color: parent.activeFocus ? SightlineTheme.primary : SightlineTheme.inputBorder }
                        }
                    }

                    Button {
                        text: "💬 Inject ST 0808 Ancillary Text (0xAC)"
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 32
                        Layout.topMargin: 4
                        background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            if (bridge) {
                                bridge.setAncillaryText(
                                    textSourceField.text,
                                    textOrigField.text,
                                    textBodyField.text,
                                    root.selectedDisplayId
                                );
                            }
                        }
                    }
                }
            }
        }

        // =========================================================================
        // TAB 5: VMTI & TARGET IMAGE CHIPS (0x84, 0xBF, 0xAD - ST 0903)
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: root.activeTab === 5

            // VMTI Fields (0xBF)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: vmtiCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: vmtiCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "MISB ST 0903 VMTI FIELD FILTERING & ONTOLOGY RATE (MESSAGE ID 0xBF)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0903 VMTI Target Telemetry"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Selectively enables sub-fields embedded into the ST 0903 Video Moving Target Indicator (VMTI) Local Set to optimize metadata bandwidth."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        Text { text: "Active VMTI Fields:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

                        CheckBox { id: vmtiBboxCheck; text: "Bounding Box (0x01)"; checked: true }
                        CheckBox { id: vmtiConfCheck; text: "Confidence (0x02)"; checked: true }
                        CheckBox { id: vmtiVelCheck; text: "Velocity (0x04)"; checked: true }
                        CheckBox { id: vmtiGeoCheck; text: "Geodetic Lat/Lon (0x08)"; checked: true }
                        CheckBox { id: vmtiOntCheck; text: "VObject Ontology (0x10)"; checked: true }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Ontology Series Update Rate:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 200 }

                        SpinBox {
                            id: ontologyRateSpin
                            from: 1; to: 300; value: 30
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 30
                        }
                        Text { text: "frames (1 = every frame, 30 = ~1 Hz)"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                    }

                    Button {
                        text: "🤖 Apply VMTI Fields (0xBF)"
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 32
                        Layout.topMargin: 4
                        background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            if (bridge) {
                                var mask = 0;
                                if (vmtiBboxCheck.checked) mask |= 0x01;
                                if (vmtiConfCheck.checked) mask |= 0x02;
                                if (vmtiVelCheck.checked) mask |= 0x04;
                                if (vmtiGeoCheck.checked) mask |= 0x08;
                                if (vmtiOntCheck.checked) mask |= 0x10;

                                bridge.setVmtiFields(mask, ontologyRateSpin.value, root.selectedDisplayId);
                            }
                        }
                    }
                }
            }

            // VMTI Image Chips (0xAD)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: chipCol.implicitHeight + 28
                color: SightlineTheme.surfaceCard
                radius: SightlineTheme.radiusMedium
                border.color: SightlineTheme.cardBorder
                border.width: 1

                ColumnLayout {
                    id: chipCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "VMTI TARGET IMAGE CHIP EXTRACTION & COMPRESSION (MESSAGE ID 0xAD)"
                            color: SightlineTheme.primary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "MISB ST 0903 VTarget Image Packets"
                            color: SightlineTheme.textMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    Text {
                        text: "Automatically crops target snapshot chips, compresses them using JPEG or PNG, and embeds them into the KLV stream for downstream AI exploitation and ATR."
                        color: SightlineTheme.textSecondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Extraction Mode:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

                        ComboBox {
                            id: chipModeCombo
                            model: ["0: Disabled", "1: Primary Target Only", "2: All Validated Tracks"]
                            currentIndex: 1
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 30
                        }

                        Text { text: "Image Format:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        ComboBox {
                            id: chipFmtCombo
                            model: ["0: JPEG Compressed", "1: PNG Lossless"]
                            currentIndex: 0
                            Layout.preferredWidth: 180
                            Layout.preferredHeight: 30
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Sizing & Resolution:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

                        Text { text: "Type:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        ComboBox {
                            id: chipSizeTypeCombo
                            model: ["0: Fixed Square", "1: Native Aspect Ratio"]
                            currentIndex: 0
                            Layout.preferredWidth: 160
                            Layout.preferredHeight: 30
                        }

                        Text { text: "Size Hint (px):"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        SpinBox {
                            id: chipSizeSpin
                            from: 32; to: 512; stepSize: 16; value: 128
                            Layout.preferredWidth: 95
                            Layout.preferredHeight: 30
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Rate Limits:"; color: SightlineTheme.textSecondary; font.pixelSize: 12; Layout.preferredWidth: 150 }

                        Text { text: "Max Chips/Frame:"; color: SightlineTheme.textMuted; font.pixelSize: 11 }
                        SpinBox {
                            id: chipMaxSpin
                            from: 1; to: 10; value: 3
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 30
                        }

                        Text { text: "Min Frames Between:"; color: SightlineTheme.textMuted; font.pixelSize: 11; Layout.leftMargin: 8 }
                        SpinBox {
                            id: chipMinFramesSpin
                            from: 0; to: 120; value: 30
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 30
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Button {
                        text: "🖼️ Apply Target Image Chip Configuration (0xAD)"
                        Layout.preferredWidth: 320
                        Layout.preferredHeight: 32
                        Layout.topMargin: 4
                        background: Rectangle { color: parent.hovered ? "#00c4d6" : SightlineTheme.primary; radius: 4 }
                        contentItem: Text { text: parent.text; color: "#0e1014"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: {
                            if (bridge) {
                                bridge.setVmtiChips(
                                    chipModeCombo.currentIndex,
                                    chipFmtCombo.currentIndex,
                                    chipSizeTypeCombo.currentIndex,
                                    chipSizeSpin.value,
                                    chipMaxSpin.value,
                                    chipMinFramesSpin.value,
                                    root.selectedDisplayId
                                );
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
