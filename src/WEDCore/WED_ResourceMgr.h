/*
 * Copyright (c) 2009, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef WED_ResourceMgr_H
#define WED_ResourceMgr_H

/*
	WED_ResourceMgr - THEORY OF OPERATION

	This class maintains a lazy-create cache or art asset previews.  It currently supports two art asset classes:
	- OBJ
	- POL

	In the case of OBJ, we use the OBJ_ package for preview and data management, thus an OBJ preview is an XObj8 struct.
	For .pol since there is no package for .pol preview (since it is somewhat trivial) we define a struct.

	HERE'S THE HACK

	Traditionally the UI interface for WED is firewalled off from the document class/implementation using a purely virtual
	abstract interface.  (See ILibrarian.h for example.)  But...I have not had the time to do this here yet.  So
	WED_LibraryMgr is used directly as sort of its own interface.  This is definitely a hack, it's definitely "wrong", but
	it's also definitely not very dangerous at this point in the code's development - that is, WED is not so big that this
	represents a scalability issue.

*/

#include "GUI_Listener.h"
#include "GUI_Broadcaster.h"
#include "IBase.h"
#include "XObjDefs.h"
#include "CompGeomDefs2.h"

class	WED_LibraryMgr;

struct	pol_info_t {
	string		base_tex; //Relative path
	bool		hasDecal;
	float		proj_s;
	float		proj_t;
	bool		kill_alpha;
	bool		wrap;
	string		group;
	int			group_offset;
	float		latitude;
	float		longitude;
	double		height_Meters;
	int			ddsHeight_Pxls;
	vector <Bbox2>	mSubBoxes;       // for subTexture selection in PreviewPanel
	Bbox2		mUVBox;              // set by PreviewPanel from selected subTexture
};

struct fac_info_t;
#include "WED_FacadePreview.h"

struct	fac_info_t : public FacadeLOD_t  {
	
	bool		is_new;       // set if version 1000, aka type 2
	string		wall_tex;
	string		roof_tex;
	bool		is_ring; 	  // can be drawn as open polygon
	
	// V1 only
	vector<FacadeLOD_t>		lods;  // WED does not recognize anything but the LOD that starts at 0

	xint					roof_surface;
	xint					wall_surface;
	bool					doubled;
	int						min_floors;	// new in 10.20 - for floor count determination, this clamps the floor range.
	int						max_floors;	
	
	// V2 only
	list<REN_facade_floor_t>floors;
	vector<asset_range>		objs;
	vector<obj_ref>			assets;
	xflt					roof_scale_s;
	xflt					roof_scale_t;
	
	// Facade Scrapers
	vector<REN_facade_scraper_t>	scrapers;
	
	// WED only
	vector<string>	w_nam;        // wall names, for property window etc
	vector<string>	w_use;        // purpose of wall, for display in preview window

	vector<XObj8 *> previews;    // thats going away. We'd rather extrude a facade from a poolygon definition

};

struct	lin_info_t {
	string		base_tex;
	float		scale_s;
	float		scale_t;
	float		eff_width;
	float		rgb[3];
	vector<float>	s1,sm,s2;
};

struct	str_info_t {
	float		offset;
	float		rotation;
	vector<XObj8 *> previews;
};

struct	road_info_t {
	map<int, string>	vroad_types;
};

#if AIRPORT_ROUTING
struct agp_t {
	struct obj {
		double  x,y,r;			// annotation position
		int		show_lo,show_hi;
		string	name;
	};
	string			base_tex;
	string			mesh_tex;
	int				hide_tiles;
	vector<double>	tile;	// the base tile in x,y,s,t quads.
	vector<obj>		objs;
};
#endif


class WED_ResourceMgr : public GUI_Broadcaster, public GUI_Listener, public virtual IBase {
public:

					 WED_ResourceMgr(WED_LibraryMgr * in_library);
					~WED_ResourceMgr();

			void	Purge(void);

			bool	GetFac(const string& path, fac_info_t& out_info, int variant =0);
			bool	GetPol(const string& path, pol_info_t& out_info);
			bool 	SetPolUV(const string& path, Bbox2 box);
			bool	GetLin(const string& path, lin_info_t& out_info);
			bool	GetStr(const string& path, str_info_t& out_info);
			bool	GetFor(const string& path, XObj8 *& obj);
			int		GetNumVariants(const string& path);

			//path is a RELATIVE PATH
			void	WritePol(const string& abspath, const pol_info_t& out_info); // side note: shouldn't this be in_info?
			bool	GetObj(const string& path, XObj8 *& obj, int variant = 0);
			bool	GetObjRelative(const string& obj_path, const string& parent_path, XObj8 *& obj);
#if AIRPORT_ROUTING
			bool	GetAGP(const string& path, agp_t& out_info);
			bool	GetRoad(const string& path, road_info_t& out_info);
#endif			


	virtual	void	ReceiveMessage(
							GUI_Broadcaster *		inSrc,
							intptr_t				inMsg,
							intptr_t				inParam);

private:
	
	map<string,vector<fac_info_t> > mFac;
	map<string,pol_info_t>		mPol;
	map<string,lin_info_t>		mLin;
	map<string,str_info_t>		mStr;
	map<string,XObj8 *>			mFor;
	map<string,vector<XObj8 *> > mObj;

#if AIRPORT_ROUTING	
	map<string,agp_t>			mAGP;
	map<string,road_info_t>		mRoad;
#endif	
	WED_LibraryMgr *			mLibrary;
};	

#endif /* WED_ResourceMgr_H */
