# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: color.tcl,v 1.1 2005/08/19 23:16:47 bseshas Exp $

proc color_globals {} {
	global colors colorid alphaid
	set colors {F0F8FF FAEBD7 00FFFF 7FFFD4 F0FFFF F5F5DC FFE4C4 FFEBCD 8A2BE2 DEB887 5F9EA0 7FFF00 D2691E FF7F50 6495ED FFF8DC DC143C 00FFFF 008B8B B8860B A9A9A9 BDB76B 556B2F FF8C00 E9967A 8FBC8F 00CED1 9400D3 FF1493 00BFFF 696969 1E90FF B22222 FFFAF0 228B22 FF00FF DCDCDC F8F8FF FFD700 DAA520 808080 008000 ADFF2F F0FFF0 FF69B4 CD5C5C FFFFF0 F0E68C E6E6FA FFF0F5 7CFC00 FFFACD ADD8E6 F08080 E0FFFF FAFAD2 90EE90 D3D3D3 FFB6C1 FFA07A 20B2AA 87CEFA 778899 B0C4DE FFFFE0 00FF00 32CD32 FAF0E6 FF00FF 66CDAA BA55D3 9370D8 3CB371 7B68EE 00FA9A 48D1CC C71585 F5FFFA FFE4E1 FFE4B5 FFDEAD FDF5E6 808000 688E23 FFA500 FF4500 DA70D6 EEE8AA 98FB98 AFEEEE D87093 FFEFD5 FFDAB9 CD853F FFC0CB DDA0DD B0E0E6 800080 FF0000 BC8F8F 4169E1 8B4513 FA8072 F4A460 2E8B57 FFF5EE A0522D C0C0C0 87CEEB 6A5ACD 708090 FFFAFA 00FF7F 4682B4 D2B48C 008080 D8BFD8 FF6347 40E0D0 EE82EE F5DEB3 F5F5F5 FFFF00 9ACD32}
	set colorid -1
	set alphaid -1
}

proc color_reset {} {
	global colorid alphaid
	set colorid -1
	set alphaid -1
}

proc unique_color {} {
	global colors colorid
	if {[info exists colorid]==0} {
		color_globals
	}
	return [lindex $colors [incr colorid]]
}

proc unique_alphabet {} {
	global alphaid
	set alphalist {0 1 2 3 4 5 6 7 8 9 A B C D E F G H I J K L M N P Q R S T U V W Y Z}
	return [lindex $alphalist [incr alphaid]]
}
