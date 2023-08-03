--[[
Copyright (c) 2006-2023 LOVE Development Team

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
--]]

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
