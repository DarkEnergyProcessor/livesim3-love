-- Make sure love exists.
local love = require("love")

function love.nogame()
	function love.load()
		love.graphics.setBackgroundColor(43/255, 165/255, 223/255)
	end

	function love.conf(t)
		t.title = "L\195\150VE " .. love._version .. " (" .. love._version_codename .. ") (NO GAME)"
		t.gammacorrect = true
		t.modules.audio = false
		t.modules.sound = false
		t.modules.joystick = false
		t.window.resizable = true
		t.window.highdpi = true

		if love._os == "iOS" then
			t.window.borderless = true
		end
	end
end

return love.nogame
