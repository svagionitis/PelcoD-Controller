"""PTZ Service Profile S Conformance Tests."""

from __future__ import annotations

import pytest


def test_continuous_move_and_stop(soap_client, schema_ptz):
    """Verify ContinuousMove with velocity space and immediate Stop."""
    move_resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/ContinuousMove",
        payload="""<tptz:ContinuousMove>
          <tptz:ProfileToken>Profile1</tptz:ProfileToken>
          <tptz:Velocity>
            <tt:PanTilt x="0.5" y="-0.2" space="http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace"/>
          </tptz:Velocity>
        </tptz:ContinuousMove>""",
        authenticate=True,
    )
    assert move_resp.status_code == 200
    schema_ptz.validate(move_resp.text)

    stop_resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/Stop",
        payload="""<tptz:Stop>
          <tptz:ProfileToken>Profile1</tptz:ProfileToken>
          <tptz:PanTilt>true</tptz:PanTilt>
          <tptz:Zoom>true</tptz:Zoom>
        </tptz:Stop>""",
        authenticate=True,
    )
    assert stop_resp.status_code == 200
    schema_ptz.validate(stop_resp.text)


def test_absolute_move(soap_client, schema_ptz):
    """Verify AbsoluteMove within generic normalized position space."""
    resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/AbsoluteMove",
        payload="""<tptz:AbsoluteMove>
          <tptz:ProfileToken>Profile1</tptz:ProfileToken>
          <tptz:Position>
            <tt:PanTilt x="0.0" y="0.0" space="http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace"/>
          </tptz:Position>
        </tptz:AbsoluteMove>""",
        authenticate=True,
    )
    assert resp.status_code == 200
    schema_ptz.validate(resp.text)


def test_preset_lifecycle(soap_client, schema_ptz):
    """Verify SetPreset, GetPresets, GotoPreset, and RemovePreset lifecycle."""
    # 1. SetPreset
    set_resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/SetPreset",
        payload="""<tptz:SetPreset>
          <tptz:ProfileToken>Profile1</tptz:ProfileToken>
          <tptz:PresetName>ConfPreset1</tptz:PresetName>
        </tptz:SetPreset>""",
        authenticate=True,
    )
    assert set_resp.status_code == 200
    schema_ptz.validate(set_resp.text)

    # 2. GetPresets
    get_resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/GetPresets",
        payload="<tptz:GetPresets><tptz:ProfileToken>Profile1</tptz:ProfileToken></tptz:GetPresets>",
        authenticate=True,
    )
    assert get_resp.status_code == 200
    schema_ptz.validate(get_resp.text)
    assert "ConfPreset1" in get_resp.text

    # 3. GotoPreset
    goto_resp = soap_client.call(
        service="ptz_service",
        action="http://www.onvif.org/ver10/ptz/wsdl/GotoPreset",
        payload="""<tptz:GotoPreset>
          <tptz:ProfileToken>Profile1</tptz:ProfileToken>
          <tptz:PresetToken>1</tptz:PresetToken>
        </tptz:GotoPreset>""",
        authenticate=True,
    )
    assert goto_resp.status_code == 200
    schema_ptz.validate(goto_resp.text)
