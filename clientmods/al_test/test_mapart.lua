function test_mapart(T)
	T.run("core.decode_image exists", function()
		T.assert(type(core.decode_image) == "function",
			"core.decode_image should be a function")
	end)

	T.run("core.write_file exists", function()
		T.assert(type(core.write_file) == "function",
			"core.write_file should be a function")
	end)

	T.run("decode_image fails on empty data", function()
		local ok, err = pcall(core.decode_image, "")
		T.assert(not ok, "should error on empty data")
	end)

	T.run("decode_image fails on garbage data", function()
		local r, img = core.decode_image("not a png")
		T.assert(r == nil, "should return nil for garbage")
	end)

	T.run("decode_image roundtrip with encode_png", function()
		-- Create a small test image
		local w, h = 4, 4
		local px = {}
		for i = 1, w * h do
			table.insert(px, 255) -- R
			table.insert(px, 0)   -- G
			table.insert(px, 0)   -- B
			table.insert(px, 255) -- A
		end
		local png = core.encode_png(w, h, px)
		T.assert(png ~= nil, "encode_png should succeed")

		local r, img = core.decode_image(png)
		T.assert(r and img, "decode_image should succeed on valid PNG")
		T.assert_eq(img.width, w, "width should match")
		T.assert_eq(img.height, h, "height should match")
		T.assert_eq(#img.data, w * h * 4, "data length should be w*h*4")

		-- Check first pixel is red
		local r_byte = string.byte(img.data, 1)
		local g_byte = string.byte(img.data, 2)
		local b_byte = string.byte(img.data, 3)
		local a_byte = string.byte(img.data, 4)
		T.assert_eq(r_byte, 255, "red channel")
		T.assert_eq(g_byte, 0, "green channel")
		T.assert_eq(b_byte, 0, "blue channel")
		T.assert_eq(a_byte, 255, "alpha channel")
	end)

	T.run("color match via encode->decode->convert pipeline", function()
		local px = {255, 0, 0, 255}
		local png_data = core.encode_png(1, 1, px)
		T.assert(png_data ~= nil, "encode_png should succeed")
		local ok, img = core.decode_image(png_data)
		T.assert(ok and img, "decode_image should succeed")

		T.assert_eq(img.width, 1, "width")
		T.assert_eq(img.height, 1, "height")
		T.assert_eq(#img.data, 4, "one RGBA pixel")

		local r = string.byte(img.data, 1)
		local g = string.byte(img.data, 2)
		local b = string.byte(img.data, 3)
		T.assert_eq(r, 255, "red channel")
		T.assert_eq(g, 0, "green channel")
		T.assert_eq(b, 0, "blue channel")
	end)

	T.run("write_file and read_file roundtrip", function()
		local test_data = "hello mapart"
		local test_path = "/tmp/antilua_mapart_test"
		local ok = core.write_file(test_path, test_data)
		T.assert(ok, "write_file should succeed")

		local ok2, data = pcall(core.read_file, test_path)
		T.assert(ok2, "read_file of written file should succeed")
		T.assert_eq(data, test_data, "read back data should match")

		os.remove(test_path)
	end)

	T.run("write_file path traversal denied", function()
		local ok, err = core.write_file("../../etc/passwd", "hack")
		T.assert(not ok, "path traversal should be denied")
	end)
end
