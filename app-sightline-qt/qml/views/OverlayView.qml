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

    readonly property int minContentWidth: 540

    // Notification toast timer
    property string statusToastText: ""
    Timer {
        id: toastTimer
        interval: 3500
        onTriggered: root.statusToastText = ""
    }

    function showToast(msg) {
        root.statusToastText = msg;
        toastTimer.restart();
    }

    ColumnLayout {
        id: mainCol
        x: 16
        width: Math.max(root.availableWidth - 32, root.minContentWidth)
        spacing: 16

        Item { Layout.preferredHeight: 2 }

        // Header with Camera Selection
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle { width: 4; height: 20; color: SightlineTheme.primary; radius: 2 }

            Text {
                text: "GRAPHIC OVERLAYS & RETICLES"
                color: SightlineTheme.textPrimary
                font.pixelSize: SightlineTheme.fontSizeMedium
                font.bold: true
                font.letterSpacing: 1.0
            }

            Text {
                text: "// Module 0x62 HUD & 0x9C Custom Primitives"
                color: SightlineTheme.textMuted
                font.pixelSize: SightlineTheme.fontSizeSmall
                font.family: "Monospace"
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Active Camera:"
                color: SightlineTheme.textSecondary
                font.pixelSize: SightlineTheme.fontSizeNormal
            }

            ComboBox {
                id: camCombo
                model: ["Camera 0 (Primary)", "Camera 1", "Camera 2", "Camera 3"]
                Layout.preferredWidth: 160
            }
        }

        // Notification Banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            radius: SightlineTheme.radiusSmall
            color: Qt.rgba(0.0, 0.9, 1.0, 0.12)
            border.color: SightlineTheme.primary
            border.width: 1
            visible: root.statusToastText.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text { text: "ℹ️"; font.pixelSize: 13 }
                Text {
                    text: root.statusToastText
                    color: SightlineTheme.primary
                    font.pixelSize: SightlineTheme.fontSizeNormal
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
        }

        // Metrics Flow
        Flow {
            Layout.fillWidth: true
            spacing: 10

            MetricCard {
                title: "Active Overlays"
                value: (bridge ? bridge.activeOverlayIds.length : 0) + " Objects"
                accentColor: SightlineTheme.primary
                iconText: "📊"
            }
            MetricCard {
                title: "Reticle Mode"
                value: reticleStyleCombo.currentText.split(" (")[0]
                accentColor: SightlineTheme.info
                iconText: "🎯"
            }
            MetricCard {
                title: "Cooldown State"
                value: (bridge && bridge.coolerCountdownActive) ? (bridge.coolerCountdownRemaining + "s Active") : "Ready"
                accentColor: (bridge && bridge.coolerCountdownActive) ? SightlineTheme.warning : SightlineTheme.success
                iconText: "❄️"
            }
            MetricCard {
                title: "Watermark Opacity"
                value: Math.round(logoOpacitySlider.value / 2.55) + "%"
                accentColor: SightlineTheme.accent
                iconText: "🖼️"
            }
        }

        // =====================================================================
        // CARD 1: RETICLE & SYMBOLOGY MODE (0x06)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: reticleCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: reticleCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.primary; radius: 1 }
                    Text {
                        text: "RETICLE & ON-SCREEN SYMBOLOGY (MESSAGE ID 0x06)"
                        color: SightlineTheme.primary
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text { text: "Primary Reticle:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: reticleStyleCombo
                        Layout.fillWidth: true
                        model: [
                            "Standard Crosshair (0)", "Corners Gate (1)", "Circle Reticle (2)",
                            "Cross & Circle (3)", "Line Reticle (4)", "Box Reticle (5)", "Full Mil-Dot (6)"
                        ]
                    }

                    Text { text: "Primary Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: reticleColorCombo
                        Layout.fillWidth: true
                        model: [
                            "White (0)", "Black (1)", "Light Gray (2)", "Gray (3)", "Dark Gray (4)",
                            "Light Blue (5)", "Blue (6)", "Dark Blue (7)", "Light Green (8)", "Green (9)",
                            "Dark Green (10)", "Cyan (11)", "Dark Cyan (12)", "Yellow (13)", "Orange (14)", "Red (15)"
                        ]
                    }

                    Text { text: "Secondary Reticle:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        id: secReticleStyleCombo
                        Layout.fillWidth: true
                        model: [
                            "None (0)", "Corners Gate (1)", "Circle Reticle (2)", "Cross & Circle (3)",
                            "Line Reticle (4)", "Box Reticle (5)", "Mil-Dot (6)"
                        ]
                    }

                    Text { text: "Line Thickness:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: lineThicknessSpin
                        from: 1
                        to: 8
                        value: 1
                        Layout.fillWidth: true
                    }
                }

                Text {
                    text: "Overlay Symbology Feature Flags:"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 12
                    font.bold: true
                    Layout.topMargin: 4
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 12

                    CheckBox { id: flagTrackIndex; text: "Track Index Badges"; checked: true }
                    CheckBox { id: flagReticle; text: "Reticle Display"; checked: true }
                    CheckBox { id: flagLogoWatermark; text: "Watermark Logo"; checked: true }
                    CheckBox { id: flagLandingAid; text: "Landing Aid Gate"; checked: false }
                    CheckBox { id: flagUserObjects; text: "User Graphic Objects (0x9C)"; checked: true }
                    CheckBox { id: flagCoastingDashed; text: "Dashed Coasting Symbology"; checked: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 4

                    Button {
                        text: "Apply Reticle & Symbology Mode (0x06)"
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 34
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.down ? SightlineTheme.primaryActive : (parent.hovered ? SightlineTheme.primaryHover : SightlineTheme.primary)
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (!bridge) return;
                            var primShape = reticleStyleCombo.currentIndex;
                            var primColor = reticleColorCombo.currentIndex;
                            var primRetByte = ((primShape & 0x0F) << 4) | (primColor & 0x0F);

                            var secShape = secReticleStyleCombo.currentIndex;
                            var secRetByte = ((secShape & 0x0F) << 4) | (primColor & 0x0F);

                            var mask = 0;
                            if (flagTrackIndex.checked) mask |= 0x0001;
                            if (flagReticle.checked) mask |= 0x0010;
                            if (flagLogoWatermark.checked) mask |= 0x0200;
                            if (flagLandingAid.checked) mask |= 0x0400;
                            if (flagUserObjects.checked) mask |= 0x1000;
                            if (flagCoastingDashed.checked) mask |= 0x2000;

                            var ok = bridge.setOverlayMode(camCombo.currentIndex, primRetByte, secRetByte, mask);
                            if (ok) {
                                root.showToast("Overlay mode successfully dispatched for Camera " + camCombo.currentIndex);
                            }
                        }
                    }

                    Button {
                        text: "Query Hardware (0x06)"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 34
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.getOverlayMode(camCombo.currentIndex);
                                root.showToast("Querying overlay mode for Camera " + camCombo.currentIndex);
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // =====================================================================
        // CARD 2: GRAPHIC PRIMITIVE STUDIO (0x9C)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: studioCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            property int currentTab: 0

            ColumnLayout {
                id: studioCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.info; radius: 1 }
                    Text {
                        text: "INTERACTIVE GRAPHIC PRIMITIVE STUDIO (MESSAGE ID 0x9C)"
                        color: SightlineTheme.info
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }
                }

                // Studio Sub-Tabs
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: ["Crosshair (+)", "Rectangle ([])", "Text Banner (T)", "KLV Telemetry (📡)"]
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            radius: SightlineTheme.radiusSmall
                            color: studioCol.parent.currentTab === index ? SightlineTheme.surfaceLight : Qt.rgba(1, 1, 1, 0.03)
                            border.color: studioCol.parent.currentTab === index ? SightlineTheme.primary : SightlineTheme.cardBorder
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: studioCol.parent.currentTab === index ? SightlineTheme.primary : SightlineTheme.textSecondary
                                font.pixelSize: 12
                                font.bold: studioCol.parent.currentTab === index
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: studioCol.parent.currentTab = index
                            }
                        }
                    }
                }

                // Common Studio Controls: Object ID, Coordinates, Origin
                GridLayout {
                    columns: 6
                    columnSpacing: 12
                    rowSpacing: 8
                    Layout.fillWidth: true

                    Text { text: "Object ID (1..199):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: studioObjIdSpin
                        from: 1
                        to: 199
                        value: 10
                        Layout.preferredWidth: 90
                    }

                    Text { text: "X (Col):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: studioXSpin
                        from: -2048
                        to: 2048
                        value: 320
                        Layout.preferredWidth: 100
                    }

                    Text { text: "Y (Row):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: studioYSpin
                        from: -2048
                        to: 2048
                        value: 240
                        Layout.preferredWidth: 100
                    }
                }

                CheckBox {
                    id: originUpperLeftCheck
                    text: "Upper-Left Origin (Checked: (0,0) is top-left of image; Unchecked: (0,0) is center)"
                    checked: true
                }

                // Tab 0: Cross Controls
                ColumnLayout {
                    visible: studioCol.parent.currentTab === 0
                    Layout.fillWidth: true
                    spacing: 10

                    GridLayout {
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 8
                        Layout.fillWidth: true

                        Text { text: "Arm Size (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox { id: crossSizeSpin; from: 5; to: 600; value: 30; Layout.fillWidth: true }

                        Text { text: "Line Thickness:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox { id: crossThickSpin; from: 1; to: 16; value: 2; Layout.fillWidth: true }

                        Text { text: "Cross Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: crossColorCombo
                            Layout.fillWidth: true
                            model: ["White (0)", "Black (1)", "Light Gray (2)", "Green (9)", "Cyan (11)", "Yellow (13)", "Red (15)"]
                        }
                    }
                }

                // Tab 1: Rectangle Controls
                ColumnLayout {
                    visible: studioCol.parent.currentTab === 1
                    Layout.fillWidth: true
                    spacing: 10

                    GridLayout {
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 8
                        Layout.fillWidth: true

                        Text { text: "Width (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox { id: rectWSpin; from: 4; to: 1920; value: 160; Layout.fillWidth: true }

                        Text { text: "Height (px):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox { id: rectHSpin; from: 4; to: 1080; value: 100; Layout.fillWidth: true }

                        Text { text: "Filled Interior:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        CheckBox { id: rectFilledCheck; text: "Solid Fill"; checked: false }

                        Text { text: "Border Thickness:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        SpinBox { id: rectThickSpin; from: 1; to: 16; value: 2; Layout.fillWidth: true }

                        Text { text: "Foreground (Border):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: rectFgCombo
                            Layout.fillWidth: true
                            model: ["White (0)", "Green (9)", "Cyan (11)", "Yellow (13)", "Red (15)", "Black (1)"]
                        }

                        Text { text: "Background (Fill):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: rectBgCombo
                            Layout.fillWidth: true
                            currentIndex: 1 // Black
                            model: ["Transparent (14)", "Black (1)", "Dark Gray (4)", "Blue (6)", "Dark Blue (7)"]
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Alpha Blending (0..31):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        Slider {
                            id: rectAlphaSlider
                            from: 0
                            to: 31
                            stepSize: 1
                            value: 0
                            Layout.fillWidth: true
                        }
                        Text {
                            text: Math.round(rectAlphaSlider.value) + " (" + Math.round(rectAlphaSlider.value / 31 * 100) + "%)"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }
                    }
                }

                // Tab 2: Text Banner Controls
                ColumnLayout {
                    visible: studioCol.parent.currentTab === 2
                    Layout.fillWidth: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: "Text String:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        TextField {
                            id: bannerTextField
                            text: "TACTICAL SURVEILLANCE ALPHA"
                            Layout.fillWidth: true
                            color: SightlineTheme.textPrimary
                            background: Rectangle {
                                color: SightlineTheme.surfaceLight
                                border.color: SightlineTheme.inputBorder
                                radius: SightlineTheme.radiusSmall
                            }
                        }
                    }

                    GridLayout {
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 8
                        Layout.fillWidth: true

                        Text { text: "Font Family:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: textFontCombo
                            Layout.fillWidth: true
                            model: ["Courier (0)", "Arial (1)", "Arial Bold (2)", "UserFont0 (16)", "UserFont1 (17)"]
                        }

                        Text { text: "Font Scale:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: textScaleCombo
                            Layout.fillWidth: true
                            currentIndex: 1 // 100%
                            model: ["Small 75% (24)", "Standard 100% (32)", "Medium 150% (48)", "Large 200% (64)", "Extra Large 300% (96)"]
                        }

                        Text { text: "Text Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: textFgCombo
                            Layout.fillWidth: true
                            model: ["White (0)", "Green (9)", "Cyan (11)", "Yellow (13)", "Red (15)"]
                        }

                        Text { text: "Shadow / Box Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: textBgCombo
                            Layout.fillWidth: true
                            model: ["Transparent (14)", "Black (1)", "Dark Gray (4)", "Dark Blue (7)"]
                        }
                    }
                }

                // Tab 3: KLV Telemetry Badge Controls
                ColumnLayout {
                    visible: studioCol.parent.currentTab === 3
                    Layout.fillWidth: true
                    spacing: 10

                    GridLayout {
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 8
                        Layout.fillWidth: true

                        Text { text: "KLV Field Tag:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: klvTagCombo
                            Layout.fillWidth: true
                            model: [
                                "UTC Time (Tag 1)", "Platform Heading (Tag 5)", "Platform Pitch (Tag 6)",
                                "Platform Roll (Tag 7)", "Sensor Latitude (Tag 13)", "Sensor Longitude (Tag 14)",
                                "Sensor True Alt (Tag 15)", "Slant Range (Tag 19)"
                            ]
                        }

                        Text { text: "Format Style:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: klvFormatCombo
                            Layout.fillWidth: true
                            model: [
                                "Standard / Time HMS (0)", "Day-Month-Year / Radians / Feet (1)", "Milli-Radians / DMS (2)"
                            ]
                        }

                        Text { text: "Format String:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        TextField {
                            id: klvFormatTemplateField
                            text: "%s"
                            Layout.fillWidth: true
                            color: SightlineTheme.textPrimary
                            background: Rectangle {
                                color: SightlineTheme.surfaceLight
                                border.color: SightlineTheme.inputBorder
                                radius: SightlineTheme.radiusSmall
                            }
                        }

                        Text { text: "Badge Color:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                        ComboBox {
                            id: klvColorCombo
                            Layout.fillWidth: true
                            model: ["Cyan (11)", "White (0)", "Green (9)", "Yellow (13)"]
                        }
                    }
                }

                // Studio Deploy Button
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Layout.topMargin: 6

                    Button {
                        text: "Deploy Graphic Primitive (0x9C)"
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 34
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.down ? SightlineTheme.info : (parent.hovered ? SightlineTheme.primaryHover : SightlineTheme.primary)
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (!bridge) return;
                            var cam = camCombo.currentIndex;
                            var objId = studioObjIdSpin.value;
                            var x = studioXSpin.value;
                            var y = studioYSpin.value;
                            var originUL = originUpperLeftCheck.checked;

                            if (studioCol.parent.currentTab === 0) { // Cross
                                var fgColMap = [0, 1, 2, 9, 11, 13, 15];
                                var col = fgColMap[crossColorCombo.currentIndex];
                                bridge.drawCross(cam, objId, x, y, crossSizeSpin.value, col, crossThickSpin.value, originUL);
                                root.showToast("Deployed Cross Object #" + objId + " at (" + x + ", " + y + ")");
                            } else if (studioCol.parent.currentTab === 1) { // Rectangle
                                var rectFgMap = [0, 9, 11, 13, 15, 1];
                                var rectBgMap = [14, 1, 4, 6, 7];
                                var fg = rectFgMap[rectFgCombo.currentIndex];
                                var bg = rectBgMap[rectBgCombo.currentIndex];
                                bridge.drawRectangle(cam, objId, x, y, rectWSpin.value, rectHSpin.value,
                                    rectFilledCheck.checked, fg, bg, Math.round(rectAlphaSlider.value),
                                    rectThickSpin.value, originUL);
                                root.showToast("Deployed Rectangle Object #" + objId + " (" + rectWSpin.value + "x" + rectHSpin.value + ")");
                            } else if (studioCol.parent.currentTab === 2) { // Text Banner
                                var fontIdMap = [0, 1, 2, 16, 17];
                                var scaleMap = [24, 32, 48, 64, 96];
                                var textFgMap = [0, 9, 11, 13, 15];
                                var textBgMap = [14, 1, 4, 7];
                                var fId = fontIdMap[textFontCombo.currentIndex];
                                var sc = scaleMap[textScaleCombo.currentIndex];
                                var tfg = textFgMap[textFgCombo.currentIndex];
                                var tbg = textBgMap[textBgCombo.currentIndex];
                                bridge.drawText(cam, objId, x, y, bannerTextField.text, fId, tfg, tbg, sc, sc, originUL);
                                root.showToast("Deployed Text Banner Object #" + objId);
                            } else if (studioCol.parent.currentTab === 3) { // KLV Badge
                                var tagMap = [1, 5, 6, 7, 13, 14, 15, 19];
                                var klvFgMap = [11, 0, 9, 13];
                                var tag = tagMap[klvTagCombo.currentIndex];
                                var kfg = klvFgMap[klvColorCombo.currentIndex];
                                bridge.drawKlvField(cam, objId, x, y, tag, klvFormatCombo.currentIndex,
                                    klvFormatTemplateField.text, 0, kfg, originUL);
                                root.showToast("Deployed KLV Telemetry Badge Object #" + objId);
                            }

                            // Advance next object ID automatically
                            studioObjIdSpin.value = Math.min(199, studioObjIdSpin.value + 1);
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // =====================================================================
        // CARD 3: ACTIVE OVERLAY INVENTORY & DELETION (0x68 & 0x9C)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: invCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: invCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.warning; radius: 1 }
                    Text {
                        text: "ACTIVE OVERLAY OBJECT INVENTORY (MESSAGE ID 0x68)"
                        color: SightlineTheme.warning
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }

                    Rectangle {
                        color: Qt.rgba(1, 0.6, 0, 0.15)
                        border.color: SightlineTheme.warning
                        border.width: 1
                        radius: 8
                        implicitWidth: invCountText.implicitWidth + 12
                        implicitHeight: 20
                        Text {
                            id: invCountText
                            anchors.centerIn: parent
                            text: (bridge ? bridge.activeOverlayIds.length : 0) + " Active"
                            color: SightlineTheme.warning
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Query Active IDs (0x68)"
                        Layout.preferredHeight: 28
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.getOverlayObjectsIds(camCombo.currentIndex);
                                root.showToast("Querying active overlay IDs for Camera " + camCombo.currentIndex);
                            }
                        }
                    }

                    Button {
                        text: "Clear All User Overlays"
                        Layout.preferredHeight: 28
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.error
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? Qt.rgba(1.0, 0.1, 0.2, 0.15) : Qt.rgba(1.0, 0.1, 0.2, 0.05)
                            border.color: SightlineTheme.error
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.destroyAllOverlays(camCombo.currentIndex);
                                root.showToast("Destroyed all user overlays on Camera " + camCombo.currentIndex);
                            }
                        }
                    }
                }

                // Inventory List View
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    // Empty State
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        color: SightlineTheme.surfaceLight
                        radius: SightlineTheme.radiusSmall
                        visible: !bridge || bridge.activeOverlayIds.length === 0

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            Text { text: "📋"; font.pixelSize: 14 }
                            Text {
                                text: "No active user overlay objects on camera. Deploy primitives using the Studio above."
                                color: SightlineTheme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }

                    // Active Items Repeater
                    Repeater {
                        model: bridge ? bridge.activeOverlayIds : []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 36
                            color: SightlineTheme.surfaceLight
                            radius: SightlineTheme.radiusSmall
                            border.color: SightlineTheme.cardBorder
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Rectangle {
                                    width: 22
                                    height: 22
                                    radius: 11
                                    color: Qt.rgba(0, 0.9, 1, 0.15)
                                    border.color: SightlineTheme.primary
                                    border.width: 1
                                    Text {
                                        anchors.centerIn: parent
                                        text: "#" + modelData
                                        color: SightlineTheme.primary
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Text {
                                    text: "Graphic Overlay Primitive (Object ID: " + modelData + ")"
                                    color: SightlineTheme.textPrimary
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: "Delete (0x9C)"
                                    Layout.preferredHeight: 24
                                    Layout.preferredWidth: 90
                                    contentItem: Text {
                                        text: parent.text
                                        color: SightlineTheme.error
                                        font.pixelSize: 10
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: parent.hovered ? Qt.rgba(1.0, 0.1, 0.2, 0.2) : Qt.rgba(1.0, 0.1, 0.2, 0.08)
                                        border.color: SightlineTheme.error
                                        radius: SightlineTheme.radiusSmall
                                    }
                                    onClicked: {
                                        if (bridge) {
                                            bridge.destroyOverlay(camCombo.currentIndex, modelData);
                                            root.showToast("Deleted Object #" + modelData);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // CARD 4: TACTICAL PRESETS & COOLER COUNTDOWN (EAN SECTION 10)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: coolerCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: coolerCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.success; radius: 1 }
                    Text {
                        text: "TACTICAL PRESETS & AUTOMATED COOLER COUNTDOWN (EAN SECTION 10)"
                        color: SightlineTheme.success
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }
                }

                // Quick Presets
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        text: "Preset: EAN 9.1 Center Cross"
                        Layout.preferredHeight: 30
                        Layout.fillWidth: true
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.drawCross(camCombo.currentIndex, 1, 0, 0, 25, 0, 1, false);
                                root.showToast("Deployed EAN 9.1 Cross at (0, 0, 25px)");
                            }
                        }
                    }

                    Button {
                        text: "Preset: Sensor Blackout Box"
                        Layout.preferredHeight: 30
                        Layout.fillWidth: true
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.drawBlackout(camCombo.currentIndex, 1, 640, 480);
                                root.showToast("Deployed 640x480 Sensor Blackout Box");
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                // Cooler Countdown Sequence Controller
                Text {
                    text: "Automated Sensor Cooldown Sequence:"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    text: "Implements EAN-Overlay-Graphics Section 10 procedure: renders a blackout mask over the imager, projects an updating countdown banner, and cleanly purges overlays upon completion."
                    color: SightlineTheme.textMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text { text: "Cooldown Duration:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: coolerDurationSpin
                        from: 5
                        to: 120
                        value: 15
                        Layout.preferredWidth: 90
                        enabled: !bridge || !bridge.coolerCountdownActive
                    }
                    Text { text: "seconds"; color: SightlineTheme.textMuted; font.pixelSize: 12 }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: (bridge && bridge.coolerCountdownActive) ? "Cooldown Running..." : "Start Cooldown Sequence"
                        enabled: !bridge || !bridge.coolerCountdownActive
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: "#0e1014"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.enabled ? (parent.hovered ? SightlineTheme.primaryHover : SightlineTheme.primary) : SightlineTheme.textMuted
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.startCoolerCountdown(camCombo.currentIndex, coolerDurationSpin.value);
                                root.showToast("Started " + coolerDurationSpin.value + "s Cooldown Sequence");
                            }
                        }
                    }

                    Button {
                        text: "Cancel Cooldown"
                        visible: bridge && bridge.coolerCountdownActive
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.error
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: Qt.rgba(1.0, 0.1, 0.2, 0.15)
                            border.color: SightlineTheme.error
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.cancelCoolerCountdown();
                                root.showToast("Cancelled Cooldown Sequence");
                            }
                        }
                    }
                }

                // Live Active Cooldown Banner
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 44
                    radius: SightlineTheme.radiusSmall
                    color: Qt.rgba(0.0, 0.9, 0.4, 0.12)
                    border.color: SightlineTheme.success
                    border.width: 1
                    visible: bridge && bridge.coolerCountdownActive

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 12

                        Text { text: "❄️"; font.pixelSize: 18 }

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "IMAGER COOLDOWN IN PROGRESS"
                                color: SightlineTheme.success
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: "Blackout mask active. Automatic destruction in: " + (bridge ? bridge.coolerCountdownRemaining : 0) + " seconds"
                                color: SightlineTheme.textPrimary
                                font.pixelSize: 12
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 36
                            height: 28
                            radius: 4
                            color: SightlineTheme.surfaceLight
                            border.color: SightlineTheme.success
                            Text {
                                anchors.centerIn: parent
                                text: (bridge ? bridge.coolerCountdownRemaining : 0) + "s"
                                color: SightlineTheme.success
                                font.bold: true
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // CARD 5: WATERMARK LOGO & USER FONT MANAGEMENT (0x9B & 0xAE)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: logoCol.implicitHeight + 28
            color: SightlineTheme.surfaceCard
            radius: SightlineTheme.radiusMedium
            border.color: SightlineTheme.cardBorder
            border.width: 1

            ColumnLayout {
                id: logoCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 3; height: 14; color: SightlineTheme.accent; radius: 1 }
                    Text {
                        text: "WATERMARK LOGO & USER FONT MANAGEMENT (0x9B & 0xAE)"
                        color: SightlineTheme.accent
                        font.pixelSize: SightlineTheme.fontSizeNormal
                        font.bold: true
                    }
                }

                // Watermark Logo
                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8
                    Layout.fillWidth: true

                    Text { text: "Logo Alpha / Opacity:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Slider {
                            id: logoOpacitySlider
                            from: 0
                            to: 255
                            value: 255
                            stepSize: 1
                            Layout.fillWidth: true
                        }
                        Text {
                            text: Math.round(logoOpacitySlider.value) + " (" + Math.round(logoOpacitySlider.value / 2.55) + "%)"
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }
                    }

                    Text { text: "Lower-Right Offset X:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: logoOffsetXSpin
                        from: 0
                        to: 1000
                        value: 20
                        Layout.fillWidth: true
                    }

                    Text { text: "Lower-Right Offset Y:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: logoOffsetYSpin
                        from: 0
                        to: 1000
                        value: 20
                        Layout.fillWidth: true
                    }

                    Item { }
                    Button {
                        text: "Apply Logo Parameters (0x9B)"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge) {
                                bridge.setLogoParameters(camCombo.currentIndex, Math.round(logoOpacitySlider.value),
                                    logoOffsetXSpin.value, logoOffsetYSpin.value);
                                root.showToast("Configured Logo Parameters (0x9B)");
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: SightlineTheme.cardBorder }

                // User TrueType Font Assignment (0xAE)
                Text {
                    text: "User TrueType Font Assignment (Message ID 0xAE):"
                    color: SightlineTheme.textSecondary
                    font.pixelSize: 12
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text { text: "Font Slot (0..15):"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    SpinBox {
                        id: fontSlotSpin
                        from: 0
                        to: 15
                        value: 0
                        Layout.preferredWidth: 80
                    }

                    Text { text: "Font Filename:"; color: SightlineTheme.textSecondary; font.pixelSize: 12 }
                    TextField {
                        id: fontFileNameField
                        text: "tactical_symbols.ttf"
                        Layout.fillWidth: true
                        color: SightlineTheme.textPrimary
                        background: Rectangle {
                            color: SightlineTheme.surfaceLight
                            border.color: SightlineTheme.inputBorder
                            radius: SightlineTheme.radiusSmall
                        }
                    }

                    Button {
                        text: "Assign Font (0xAE)"
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: parent.text
                            color: SightlineTheme.textPrimary
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? SightlineTheme.surfaceLight : SightlineTheme.surfaceCard
                            border.color: SightlineTheme.cardBorder
                            radius: SightlineTheme.radiusSmall
                        }
                        onClicked: {
                            if (bridge && fontFileNameField.text.trim().length > 0) {
                                bridge.setUserFont(fontSlotSpin.value, fontFileNameField.text.trim());
                                root.showToast("Assigned Font Slot " + fontSlotSpin.value + " to " + fontFileNameField.text.trim());
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 16 }
    }
}
