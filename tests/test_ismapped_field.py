"""
Test to verify isMapped field is present in videoParameters metadata.
"""

import sys
from unittest.mock import Mock


def test_ismapped_field_in_build_json():
    """
    Verify that build_json() includes isMapped field set to False in videoParameters.
    
    The isMapped field indicates whether ld-discmap has been run on the TBC.
    Raw decoder output should always set this to False.
    """
    # Import the core module
    from lddecode import core
    
    # Create a minimal mock decoder instance (LDdecode has build_json)
    decoder = Mock(spec=core.LDdecode)
    decoder.analog_audio = 48000
    decoder.fieldinfo = [{"seqNo": 1, "isFirstField": True}]
    decoder.branch = "test-branch"
    decoder.commit = "test-commit"
    decoder.blackIRE = 0
    
    # Create a mock field with required attributes
    mock_field = Mock()
    mock_field.rf = Mock()
    mock_field.rf.system = "NTSC"
    mock_field.rf.SysParams = {
        "outlinelen": 910,
        "outfreq": 14.318181818,
        "colorBurstUS": (5.3, 7.8),
        "activeVideoUS": (9.2, 63.5)
    }
    mock_field.outlinecount = 263
    
    # Mock the hz_to_output and iretohz methods
    def mock_iretohz(ire):
        return 1000000 + (ire * 10000)
    
    def mock_hz_to_output(hz):
        return hz / 1000
    
    mock_field.hz_to_output = mock_hz_to_output
    mock_field.rf.iretohz = mock_iretohz
    
    decoder.fieldstack = [mock_field]
    
    # Call build_json using the method from the class
    json_output = core.LDdecode.build_json(decoder)
    
    # Verify the structure
    assert json_output is not None, "build_json() should return a dict"
    assert "videoParameters" in json_output, "JSON should have videoParameters"
    
    video_params = json_output["videoParameters"]
    
    # Verify isMapped field exists and is False
    assert "isMapped" in video_params, "videoParameters should contain isMapped field"
    assert video_params["isMapped"] is False, "isMapped should be False for raw decoder output"
    
    # Verify it's in the correct alphabetical position (after gitCommit)
    keys = list(video_params.keys())
    git_commit_idx = keys.index("gitCommit") if "gitCommit" in keys else -1
    is_mapped_idx = keys.index("isMapped")
    system_idx = keys.index("system") if "system" in keys else len(keys)
    
    # isMapped should come after gitCommit and before system in alphabetical order
    if git_commit_idx >= 0:
        assert is_mapped_idx > git_commit_idx, "isMapped should come after gitCommit"
    if system_idx < len(keys):
        assert is_mapped_idx < system_idx, "isMapped should come before system"
    
    print("✓ isMapped field is correctly present in videoParameters")
    print(f"✓ isMapped = {video_params['isMapped']}")
    return True


if __name__ == "__main__":
    try:
        test_ismapped_field_in_build_json()
        print("\n✓ All tests passed!")
        sys.exit(0)
    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ Test error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
