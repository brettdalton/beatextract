--[[============================================================================
main.lua
============================================================================]]--

-- Placeholder for the dialog
local dialog = nil

-- Placeholder to expose the ViewBuilder outside the show_dialog() function
local vb = nil

-- Reload the script whenever this file is saved. 
-- Additionally, execute the attached function.
_AUTO_RELOAD_DEBUG = function()
  
end

-- Read from the manifest.xml file.
class "RenoiseScriptingTool" (renoise.Document.DocumentNode)
  function RenoiseScriptingTool:__init()    
    renoise.Document.DocumentNode.__init(self) 
    self:add_property("Name", "Untitled Tool")
    self:add_property("Id", "Unknown Id")
  end

local manifest = RenoiseScriptingTool()
local ok,err = manifest:load_from("manifest.xml")
local tool_name = manifest:property("Name").value
local tool_id = manifest:property("Id").value


local sample_path = renoise.Document.ObservableString()
local first_beat  = renoise.Document.ObservableString()
local tolerance  = renoise.Document.ObservableString()
local bpm_guess = renoise.Document.ObservableString()
local running_linux = false

--------------------------------------------------------------------------------
-- Main functions
--------------------------------------------------------------------------------



function get_track(pat_idx)
  return  renoise.song().patterns[pat_idx].tracks[ #renoise.song().tracks-renoise.song().send_track_count ]
end

function get_bpm_automation(pat_idx)
  local master = renoise.song().tracks[ #renoise.song().tracks-renoise.song().send_track_count ]
  local track = get_track(pat_idx)
  local bpm = master.devices[1].parameters[6]
    
  local a = track:find_automation(bpm)
  if not a then a = track:create_automation (bpm) end
  return a
end

function read_automation_slices()

  local pattern = renoise.song().selected_pattern_index
  local track = get_track(pattern)
  local auto = get_bpm_automation(pattern)
  
  local beat = 1
  
  local slices = renoise.song().selected_sample.slice_markers
  local last_slice  = slices[1]
  
  for index,slice in pairs(slices) do 
    
    print(slice-last_slice)
    
    if index == 1 then
    else
      auto.playmode = renoise.PatternTrackAutomation.PLAYMODE_POINTS
      local value = 60/( (slice-last_slice)/44100 )
      print(value)
      auto:add_point_at (beat,(value-32)/(999-32))
      beat = beat+4
      if beat >= renoise.song().patterns[pattern].number_of_lines then
        pattern = pattern+1
        track = get_track(pattern)
        auto = get_bpm_automation(pattern)
        beat = 1
      end
    end
    
    last_slice = slice
    
  end
end


local function load_sample(path)
  local count = 0
  for i,s in  pairs(renoise.song().selected_instrument.samples) do
    count = i
  end
  if count == 0 then
    renoise.song().selected_instrument:insert_sample_at(1)
  end
  renoise.song().selected_sample.sample_buffer:load_from(path)
end

local function analyze_sample(mode)
  
  renoise.song().selected_sample.sample_buffer:save_as("out/data.wav", "wav")
  
  local beats = " "
  for idx,val in pairs(renoise.song().selected_sample.slice_markers) do
    beats = beats.." "..tostring(val/44100)
  end
  
  for idx,val in pairs(renoise.song().selected_sample.slice_markers) do
    renoise.song().selected_sample:delete_slice_marker(val)
  end 

  local exe = "main.exe"  
  if running_linux==true then exe = "./main.exe" end

  --local exe = "main.exe"--"D:\\Source\\beat_extract\\main.exe"
  local command = exe.." "..mode.." ".."out/data.wav"..beats
  os.execute(command)
  local handle = io.open("out/info.txt")
  
  local idx = 0
  for line in handle:lines() do 
    if(idx<255) then
      print(line)
      renoise.song().selected_sample:insert_slice_marker(1+tonumber(line)*44100)
    else
      break
    end
    idx = idx+1
  end  
  handle:close()
end


--------------------------------------------------------------------------------
-- GUI
--------------------------------------------------------------------------------

local function show_dialog()

  -- This block makes sure a non-modal dialog is shown once.
  -- If the dialog is already opened, it will be focused.
  if dialog and dialog.visible then
    dialog:show()
    return
  end
    
  local w0= 100
  local w1= 10
  local w2= 300
  local vb = renoise.ViewBuilder()
  local content = vb:column
  {
    vb:horizontal_aligner
    {
      vb:button{text="Build Executable",notifier= function() os.execute("make") end },
	  vb:text{text="Running Linux?"},
      vb:checkbox{notifier= function(val) running_linux=val end }
    },
	
    vb:horizontal_aligner
    {
		mode= "center",
		vb:column
		{
		  margin =20,
		  style="panel",
		  vb:row {
		  vb:button {width = w0,text = "Load Sample", notifier=function() load_sample(sample_path.value) end},
		  vb:space{width=w1},
		  vb:textfield{width = w2,text = "D:\\Krantz.wav", bind = sample_path},
		  },
		  vb:row {
		  vb:text{width=w0,text="Error Tolerance:"},
		  vb:space{width=w1},
		  vb:textfield{width=w2,text = "0.15", bind = tolerance},
		  },
		  vb:row {
		  vb:text{width=w0,text="Default BPM:"},
		  vb:space{width=w1}, 
		  vb:textfield{width=w2,text = "120", bind = bpm_guess},
		  },
		},
    },
    
    vb:row
    {
      vb:button{text="Load Peaks as Slices",notifier= function() analyze_sample("PEAKS") end },
      vb:button{text="Load Beats as Slices",notifier= function () analyze_sample("BEATS") end },
      vb:button{text="Load OSS Signal",notifier=function() analyze_sample("USAGE") load_sample("out/oss.wav") end},
      vb:button{text="Slices To BPM Automation",notifier=function() read_automation_slices() end},
    }
  }

  dialog = renoise.app():show_custom_dialog("Beat Extract Tool",content)
  
end


--------------------------------------------------------------------------------
-- Menu entries
--------------------------------------------------------------------------------

renoise.tool():add_menu_entry {
    name = "Main Menu:Tools:CSC499:Beat Extract Tool",
    invoke = function () show_dialog() end
}


--------------------------------------------------------------------------------
-- Key Binding
--------------------------------------------------------------------------------

--[[
renoise.tool():add_keybinding {
  name = "Global:Tools:" .. tool_name.."...",
  invoke = show_dialog
}
--]]


--------------------------------------------------------------------------------
-- MIDI Mapping
--------------------------------------------------------------------------------

--[[
renoise.tool():add_midi_mapping {
  name = tool_id..":Show Dialog...",
  invoke = show_dialog
}
--]]
