/* 
 * Copyright (c) 2012, Laminar Research.
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

#include "WED_LibraryPreviewPane.h"

#include "ITexMgr.h"
#include "TexUtils.h"
#include "MathUtils.h"

#include "GUI_DrawUtils.h"
#include "GUI_Broadcaster.h"
#include "GUI_GraphState.h"
#include "GUI_Fonts.h"
#include "GUI_Messages.h"
#include "GUI_Resources.h"

#include "WED_Colors.h"
#include "WED_LibraryMgr.h"
#include "WED_Globals.h"
#include "WED_ResourceMgr.h"
#include "WED_PreviewLayer.h"

#include "XObjDefs.h"
#include "ObjDraw.h"

#if APL
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

enum { 
	next_variant = GUI_APP_MESSAGES
};


WED_LibraryPreviewPane::WED_LibraryPreviewPane(WED_ResourceMgr * res_mgr, ITexMgr * tex_mgr) : mResMgr(res_mgr), mTexMgr(tex_mgr),mZoom(1.0),
                               mPsi(10.0f),mThe(10.0f), mHgt(10.0f), mWid(20.0f)
{
		int k_reg[4] = { 0, 0, 1, 3 };
		int k_hil[4] = { 0, 1, 1, 3 };
		
//		int b[4]; GetBounds(b);  // No good, bounds not established at this point

		mNextButton = new GUI_Button("push_buttons.png",btn_Push,k_reg, k_hil,k_reg,k_hil);
		mNextButton->SetBounds(5,45,55,45+GUI_GetImageResourceHeight("push_buttons.png") / 3);
//		mNextButton->SetBounds(b[2]-55,b[3]-5-GUI_GetImageResourceHeight("push_buttons.png") / 3,b[2]-5,b[4]-5);
		mNextButton->SetSticky(1,1,0,0); // follow left bottom corner
		mNextButton->SetParent(this);
		mNextButton->SetMsg(next_variant,0);
		mNextButton->AddListener(this);
		mNextButton->Hide();
}

void		WED_LibraryPreviewPane::ReceiveMessage(GUI_Broadcaster * inSrc, intptr_t inMsg, intptr_t inParam)
{
	if(inMsg == next_variant)
	{
		if (mVariant < mNumVariants-1)
			mVariant++;
		else
			mVariant = 0;
		
		char s[16]; sprintf(s,"%d/%d",mVariant+1,mNumVariants);
		mNextButton->SetDescriptor(s);
	}
}

void WED_LibraryPreviewPane::SetResource(const string& r, int res_type)
{
	mRes = r;
	mType = res_type;
	mVariant = 0;
	
	if(res_type == res_Object || res_type == res_Facade) 
	{	
		mNumVariants = mResMgr->GetNumVariants(r);
		if(mNumVariants >1)
		{
			char s[16]; sprintf(s,"%d/%d",mVariant+1,mNumVariants);
			mNextButton->SetDescriptor(s);
			mNextButton->Show();
		}
		else
			mNextButton->Hide();
		mHgt = 10.0;
		mWid = 20.0;
		
		fac_info_t fac;
		mResMgr->GetFac(mRes, fac, mVariant);
		WED_MakeFacadePreview(fac, mHgt, mWid);
	}
	else
	{
		mNumVariants = 1;     // we haven't yet implemented variant display for anything else
		mNextButton->Hide();
	}

	if (res_type == res_Polygon)
	{
		int tex_x, tex_y;
		float tex_aspect;
		pol_info_t pol;

		mResMgr->GetPol(mRes,pol);
		TexRef	tref = mTexMgr->LookupTexture(pol.base_tex.c_str(),true, pol.wrap ? (tex_Compress_Ok|tex_Wrap) : tex_Compress_Ok);
		if (tref)
		{
			mTexMgr->GetTexInfo(tref,&tex_x,&tex_y,NULL, NULL, NULL, NULL);
			tex_aspect = float(pol.proj_s * tex_x) / float(pol.proj_t * tex_y);
		}
		else
			tex_aspect = 1.0;
			
		mDs = tex_aspect > 1.0 ? 1.0 : tex_aspect;
		mDt = tex_aspect > 1.0 ? 1.0/tex_aspect : 1.0;
	}
}

void WED_LibraryPreviewPane::ClearResource(void)
{
	mRes.clear();
}

int	WED_LibraryPreviewPane::ScrollWheel(int x, int y, int dist, int axis)
{
	while(dist > 0)
	{
		mZoom /= 1.2;
		--dist;
	}
	while(dist < 0)
	{
		mZoom *= 1.2;
		++dist;
	}
	mZoom=fltlim(mZoom,0.1,3.0);
	Refresh();
	return 1;
}

int	WED_LibraryPreviewPane::MouseDown(int x, int y, int button)
{
	mX = x;
	mY = y;
	mPsiOrig=mPsi;
	mTheOrig=mThe;
	mHgtOrig=mHgt;
	mWidOrig=mWid;
	
	int b[4]; GetBounds(b);
	
    if (mType == res_Polygon)
    {
		pol_info_t pol;
		mResMgr->GetPol(mRes,pol);
		
		float prev_space = min(b[2]-b[0],b[3]-b[1]);
		float ds = prev_space / mZoom * mDs;
		float dt = prev_space / mZoom * mDt;


		float x1 = 0.5 *(b[2] + b[0] - ds);         // texture left bottom corner
		float y1 = 0.5* (b[3] + b[1] - dt);

		Point2 st = Point2((x-x1)/ds, (y-y1)/dt);   // texture coodinates where we clicked at

		if (pol.mSubBoxes.size())
		{
			// go through list of subtexture boxes and find if we clicked inside one
			static int lastBox = -1;                // the box we clicked on the last time. Helps to cycle trough overlapping boxes
			int        firstBox = 999;              // the first box that fits this click location
			int n;
			for (n=0; n < pol.mSubBoxes.size(); ++n)
			{
				if (pol.mSubBoxes[n].contains(st))
				{
					if (n < firstBox) firstBox = n; // memorize the first of all boxes that fits the click
					if (n > lastBox)                // is it a new-to-us box ?
					{
						pol.mUVBox=pol.mSubBoxes[n];
						lastBox=n;
						break;
					}
				}
			}

			if (n >= pol.mSubBoxes.size())         // apparently there is no new-to-us box here
			{
				if (firstBox < 999)
				{
					pol.mUVBox=pol.mSubBoxes[firstBox];    // so we go with the first best box we found
					lastBox=firstBox;
				}
				else
				{
					pol.mUVBox = Bbox2(0,0,1,1);   // there is no box where we clicked -> select whole texture
					lastBox = -1;
				}
			}
		}
		else
			pol.mUVBox = Bbox2();                 // there are no subboxes defined at all
		
		mResMgr->SetPolUV(mRes,pol.mUVBox);
		Refresh();
	}
	return 1;
}

void	WED_LibraryPreviewPane::MouseDrag(int x, int y, int button)
{
	float dx = x - mX;
	float dy = y - mY;
	
	if(mType == res_Facade && button == 1)
	{
		float oldHgt = mHgt;
		mHgt = mHgtOrig + dy * 0.25;
		mHgt = fltlim(mHgt,0,99);
		
		float oldWid = mWid;
		mWid = mWidOrig + dx * 0.25;
		mWid = fltlim(mWid,1,99);

//printf("Drag %.1lf %.1lf\n", dx, dy);
//		if ( fabs(oldHgt-mHgt) >= 1.0 || fabs(oldWid-mWid) >= 1.0)
		{
			fac_info_t fac;
			mResMgr->GetFac(mRes, fac, mVariant);
			WED_MakeFacadePreview(fac, mHgt, mWid);
		}
	}
	else
	{
		mPsi = mPsiOrig + dx * 0.5;
		mThe = mTheOrig - dy * 0.5;
		
		mThe = fltlim(mThe,-89,89);             // prevent some vertical objects fading completely out of view
		mPsi = fltwrap(mPsi,-180-45,180-45);    // adjusted for proper annotations in WED_LibraryPreviewPane::Draw
	}
	Refresh();
}

void	WED_LibraryPreviewPane::MouseUp  (int x, int y, int button)
{
}

void	WED_LibraryPreviewPane::Draw(GUI_GraphState * g)
{
	int b[4]; GetBounds(b);

	XObj8 * o = NULL;
	vector<XObj8 *> o_vec;
	
	float dx = b[2] - b[0];
	float dy = b[3] - b[1];

	#if AIRPORT_ROUTING
	agp_t agp;
	#endif
	pol_info_t pol;
	lin_info_t lin;
	fac_info_t fac;

	if(!mRes.empty())
	{	switch(mType) {
		case res_Polygon:
			if(mResMgr->GetPol(mRes,pol))
			{
				TexRef	tref = mTexMgr->LookupTexture(pol.base_tex.c_str(),true, pol.wrap ? (tex_Compress_Ok|tex_Wrap) : tex_Compress_Ok);
				if(tref != NULL)
				{
					int tex_id = mTexMgr->GetTexID(tref);

					if (tex_id != 0)
					{
						g->SetState(false,1,false,!pol.kill_alpha,!pol.kill_alpha,false,false);
						g->BindTex(tex_id,0);
						
						float prev_space = min(b[2]-b[0],b[3]-b[1]);
						float ds = prev_space / mZoom * mDs;
						float dt = prev_space / mZoom * mDt;
						
						float x1 = (dx - ds) /2;
						float x2 = (dx + ds) /2;
						float y1 = (dy - dt) /2;
						float y2 = (dy + dt) /2;
						
						glBegin(GL_QUADS);
						if(pol.wrap)
						{
							glTexCoord2f(extrap(x1,0,x2,1,b[0]),	extrap(y1,0,y2,1,b[1]));			glVertex2f(b[0],b[1]);
							glTexCoord2f(extrap(x1,0,x2,1,b[0]),	extrap(y1,0,y2,1,b[3]));			glVertex2f(b[0],b[3]);
							glTexCoord2f(extrap(x1,0,x2,1,b[2]),	extrap(y1,0,y2,1,b[3]));			glVertex2f(b[2],b[3]);
							glTexCoord2f(extrap(x1,0,x2,1,b[2]),	extrap(y1,0,y2,1,b[1]));			glVertex2f(b[2],b[1]);
						}
						else
						{
							glTexCoord2f(0,0); glVertex2f(x1,y1);
							glTexCoord2f(0,1); glVertex2f(x1,y2);
							glTexCoord2f(1,1); glVertex2f(x2,y2);
							glTexCoord2f(1,0); glVertex2f(x2,y1);
						}
						glEnd();
						
						if (!pol.mUVBox.is_null())                   // draw a box around the selected texture area
						{
							g->Reset();
							glColor4fv(WED_Color_RGBA(wed_StructureSelected));
							glBegin(GL_LINE_LOOP);
							glVertex2f(x1 + ds * pol.mUVBox.p1.x(), y1 + dt * pol.mUVBox.p1.y());
							glVertex2f(x1 + ds * pol.mUVBox.p2.x(), y1 + dt * pol.mUVBox.p1.y());
							glVertex2f(x1 + ds * pol.mUVBox.p2.x(), y1 + dt * pol.mUVBox.p2.y());
							glVertex2f(x1 + ds * pol.mUVBox.p1.x(), y1 + dt * pol.mUVBox.p2.y());
							glEnd();
						}
					}	
				}
			}
			break;
		case res_Line:
			if(mResMgr->GetLin(mRes,lin))
			{
				TexRef	tref = mTexMgr->LookupTexture(lin.base_tex.c_str(),true, tex_Compress_Ok);
				if(tref != NULL)
				{
					int tex_id = mTexMgr->GetTexID(tref);

					if (tex_id != 0)
					{
						g->SetState(false,1,false,true,true,false,false);
						g->BindTex(tex_id,0);
						
						// always fit into vertical size of window
						float dt = (b[3]-b[1]) / mZoom;
						
						glBegin(GL_QUADS);
						for (int n=0; n<lin.s1.size(); ++n)
						{	
							float ds = dt * (lin.scale_s * (lin.s2[n]-lin.s1[n]) / lin.scale_t);
							
							float x1 = (dx - ds) /2;
							float x2 = (dx + ds) /2;
							float y1 = (dy - dt) /2;
							float y2 = (dy + dt) /2;
							
							glTexCoord2f(lin.s1[n], 0); glVertex2f(x1,y1);
							glTexCoord2f(lin.s1[n], 1); glVertex2f(x1,y2);
							glTexCoord2f(lin.s2[n],1); glVertex2f(x2,y2);
							glTexCoord2f(lin.s2[n],0); glVertex2f(x2,y1);
						}
						glEnd();
					}	
				}
			}
			break;
		case res_Forest:
			if(!mResMgr->GetFor(mRes,o))
				break;
		case res_String:
			if(!o)
			{
				str_info_t str;
				if(mResMgr->GetStr(mRes,str))
					o = str.previews[0];             // do the cheap thing: show only the first object. Could show a whole line ...
			}
#if 1 
		// facade preview, not yet ready for primetime
		case res_Facade:
			if(!o)
			{
				if (mResMgr->GetFac(mRes,fac,mVariant))
				{
					if (fac.previews.size()) o = fac.previews[0];
					o_vec = fac.previews;
				}
				else
					break;
			}
#endif
		case res_Object:
			float sx = ((dx > dy) ? (dx / dy) : 1.0)/2;
			float sy = ((dx > dy) ? 1.0 : (dy / dx))/2;

			if (o || mResMgr->GetObj(mRes,o,mVariant))
			{
				float real_radius=pythag(
									o->xyz_max[0]-o->xyz_min[0],
									o->xyz_max[1]-o->xyz_min[1],
									o->xyz_max[2]-o->xyz_min[2]);
				float approx_radius = real_radius * mZoom;

				glPushAttrib(GL_VIEWPORT_BIT);
				glViewport(b[0],b[1],b[2]-b[0],b[3]-b[1]);
				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();
				glOrtho(sx * -approx_radius,sx * approx_radius,sy * -approx_radius,sy * approx_radius,-real_radius,real_radius);
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadIdentity();			
				glRotatef(mThe,1,0,0);
				glRotatef(mPsi,0,1,0);
				g->EnableDepth(true,true);
				glClear(GL_DEPTH_BUFFER_BIT);

				if (o_vec.size() < 1) o_vec.push_back(o);
//				for (vector<XObj8 *>::iterator i = o_vec.begin(); i != o_vec.end(); ++i)
					draw_obj_at_xyz(mTexMgr, o_vec[0],
						-(o->xyz_max[0]+o->xyz_min[0])*0.5f,
						-(o->xyz_max[1]+o->xyz_min[1])*0.5f,
						-(o->xyz_max[2]+o->xyz_min[2])*0.5f,
						0, g);
						
				if(o_vec.size() > 1)
					draw_obj_at_xyz(mTexMgr, o_vec[1],
						-(o->xyz_max[0]+o->xyz_min[0])*0.5f,
						-(o->xyz_max[1]+o->xyz_min[1])*0.5f,
						-(o->xyz_max[2]+o->xyz_min[2])*0.5f,
						0, g);

					
				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				glPopAttrib();
			}
			#if AIRPORT_ROUTING
			else if (mResMgr->GetAGP(mRes,agp))
			{
				double min_xy[2] = { 0, 0 };
				double max_xy[2] = { 0, 0 };
				for(int n = 0; n < agp.tile.size(); n += 4)
				{
					min_xy[0] = min(min_xy[0],agp.tile[n]);
					max_xy[0] = max(max_xy[0],agp.tile[n]);
					min_xy[1] = min(min_xy[1],agp.tile[n+1]);
					max_xy[1] = max(max_xy[1],agp.tile[n+1]);
				}

				float real_radius=pythag(
									max_xy[0] - min_xy[0],
									max_xy[1] - min_xy[1]);
									
				float approx_radius = real_radius * mZoom;

				glPushAttrib(GL_VIEWPORT_BIT);
				glViewport(b[0],b[1],b[2]-b[0],b[3]-b[1]);
				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();
				glOrtho(sx * -approx_radius,sx * approx_radius,sy * -approx_radius,sy * approx_radius,-real_radius,real_radius);
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadIdentity();			
				glRotatef(mThe,1,0,0);
				glRotatef(mPsi,0,1,0);
				g->EnableDepth(true,true);
				glClear(GL_DEPTH_BUFFER_BIT);
				
				glTranslatef((max_xy[0]+min_xy[0]) * -0.5,
							  0.0,
							(max_xy[1]+min_xy[1]) * 0.5);

					g->SetState(false,1,false,true,true,false,false);
					TexRef	ref = mTexMgr->LookupTexture(agp.base_tex.c_str() ,true, tex_Linear|tex_Mipmap|tex_Compress_Ok);			
					int id1 = ref  ? mTexMgr->GetTexID(ref ) : 0;
					if(id1)g->BindTex(id1,0);

					glColor3f(1,1,1);
					if(!agp.tile.empty() && !agp.hide_tiles)
					{
						glDisable(GL_CULL_FACE);
						glBegin(GL_TRIANGLE_FAN);
						for(int n = 0; n < agp.tile.size(); n += 4)
						{
							glTexCoord2f(agp.tile[n+2],agp.tile[n+3]);
							glVertex3f(agp.tile[n],0,-agp.tile[n+1]);
						}
						glEnd();
						glEnable(GL_CULL_FACE);
					}	
					for(vector<agp_t::obj>::iterator o = agp.objs.begin(); o != agp.objs.end(); ++o)
					{
						XObj8 * oo;
						if(mResMgr->GetObjRelative(o->name,mRes,oo))
						{
							draw_obj_at_xyz(mTexMgr, oo, o->x,0,-o->y,o->r, g);			
						} 
					}

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				glPopAttrib();			
			}
			#endif
			break;
		}
		
		// plot some additional information about the previewed object
		char buf1[100] = "", buf2[100] = "";
		switch(mType)
		{
			case res_Facade:
				if (fac.previews.size() && fac.walls.size())
				{
					int side = (135-mPsi)/90.0;
					if (side >= fac.w_nam.size()) side=0;
					sprintf(buf1,"Facing Wall \'%s\' intended for %s h/w=%.1lf/%.1lf", fac.w_nam[side].c_str(), fac.w_use[side].c_str(), mHgt, mWid);
					
					int n_wall = fac.walls.size();
					int j = sprintf(buf2,"Type %d, %d wall%s ", fac.is_new ? 2 : 1, n_wall, n_wall > 1 ? "s" : "");

					if (fac.min_floors < 0) 
						sprintf(buf2+j,", h=%d %c only", fac.max_floors / (gIsFeet ? 0.3048 : 1), gIsFeet ? '\'' : 'm');
					else
						sprintf(buf2+j,", h=%d to %d", fac.min_floors, fac.max_floors);
				}
				else
					sprintf(buf2,"No preview for this facde type available, yet");
				break;
			case res_Polygon:
				if (pol.hasDecal)
				{
					sprintf(buf1,"Has decal (not shown)");
				}
				if (pol.mSubBoxes.size())
				{
					sprintf(buf2,"Select desired part of texture by clicking on it");
				}
				break;
			case res_Line:
				if (lin.s1.size() && lin.s2.size())
				{ 
					sprintf(buf2,"w~%.0f%s",lin.eff_width * (gIsFeet ? 100.0/2.54 : 100.0), gIsFeet ? "in" : "cm" );
				}
				break;
			case res_Object:
			case res_Forest:
			case res_String:
				if (o)
				{
					int n = sprintf(buf2,"max h=%.1f%s", o->xyz_max[1] / (gIsFeet ? 0.3048 : 1.0), gIsFeet ? "'" : "m");
					if (o->xyz_min[1] < -0.07)
						sprintf(buf2+n,", below ground to %.1f%s", o->xyz_min[1] / (gIsFeet ? 0.3048 : 1.0), gIsFeet ? "'" : "m");
				}
				break;
		}
		float text_color[4] = { 1,1,1,1 };
		if (buf1[0])
			GUI_FontDraw(g, font_UI_Basic, text_color, b[0]+5,b[1]+25, buf1);
		if (buf2[0])
			GUI_FontDraw(g, font_UI_Basic, text_color, b[0]+5,b[1]+10, buf2);
	}
}
