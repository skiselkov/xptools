/*
 * Copyright (c) 2004, Laminar Research.
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
#include "NetTables.h"
#include "EnumSystem.h"
#include "ConfigSystem.h"
#include "AssertUtils.h"
#include "XESConstants.h"
#include <math.h>

NetFeatureInfoTable				gNetFeatures;
NetRepInfoTable					gNetReps;
Feature2RepInfoTable			gFeature2Rep;
ForkRuleTable					gForkRules;
ChangeRuleTable					gChangeRules;
BridgeInfoTable					gBridgeInfo;


bool	RoadGeneralProps(const vector<string>& tokens, void * ref)
{
	int				feature_type;
	NetFeatureInfo	info;
	if (TokenizeLine(tokens, " efe", &feature_type,
		&info.density_factor, &info.oneway_feature) != 4) return false;


	if (gNetFeatures.count(feature_type) > 0)
		printf("WARNING: duplicate token %s\n", tokens[1].c_str());


	gNetFeatures[feature_type] = info;
	return true;
}

bool	ReadRoadSpecificProps(const vector<string>& tokens, void * ref)
{
	int rep_type;
	NetRepInfo	info;
	info.export_type_draped = NO_VALUE;	// hack for mesh tool - allow draped param to not be attached!

	float	crease, max_rad;

	if (TokenizeLine(tokens, " efffeiifff",&rep_type,
		&info.width, &info.pad, &info.building_percent, &info.use_mode, &info.is_oneway, &info.export_type_draped, &crease, &max_rad, &info.max_err) != 11)
	{
		return false;
	}
	
	info.crease_angle_cos=cos(crease * DEG_TO_RAD);
	info.min_defl_deg_mtr = max_rad > 0.0 ? (360.0 / (2 * PI * max_rad)) : 0.0f;
	
	if (gNetReps.count(rep_type) > 0)
		printf("WARNING: duplicate token %s\n", FetchTokenString(rep_type));

	gNetReps[rep_type] = info;
	return true;
}

bool	ReadRoadPick(const vector<string>& tokens, void * ref)
{
	Feature2RepInfo	info;
	int				feature_type;

	if (TokenizeLine(tokens, " effeeffffe", &feature_type, 
		&info.min_density,&info.max_density, 
		&info.zoning_left,
		&info.zoning_right,
		&info.rain_min,
		&info.rain_max,
		&info.temp_min,
		&info.temp_max,
		&info.rep_type) != 11)	return false;

	gFeature2Rep.insert(Feature2RepInfoTable::value_type(feature_type, info));
	return true;
}

bool	ReadForkRule(const vector<string>& tokens, void * ref)
{
	ForkRule r;
	if(TokenizeLine(tokens," eeeeee",
						&r.trunk,
						&r.left,
						&r.right,
						&r.new_trunk,
						&r.new_left,
						&r.new_right) != 7)
	return false;
	gForkRules.push_back(r);
	return true;
				
}

bool	ReadChangeRule(const vector<string>& tokens, void * ref)
{
	ChangeRule r;
	if(TokenizeLine(tokens," eeee",
						&r.prev,
						&r.next,
						&r.new_mid) != 4)
	return false;
	gChangeRules.push_back(r);
	return true;
				
}


bool	ReadRoadBridge(const vector<string>& tokens, void * ref)
{
	BridgeInfo	info;

	if (TokenizeLine(tokens, " efffffffififfffffffi",
		&info.rep_type,
		&info.min_length, &info.max_length,
		&info.min_seg_length, &info.max_seg_length,
		&info.min_seg_count, &info.max_seg_count,
		&info.curve_limit,
		&info.split_count, &info.split_length, &info.split_arch,
		&info.min_start_agl, &info.max_start_agl,
		&info.search_dist,   &info.pref_start_agl,
		&info.min_center_agl, &info.max_center_agl,
		&info.height_ratio, &info.road_slope,
		&info.export_type) != 21) return false;

		// Special case these - otherwise we get inexact values from deg-rad conversion.
		 if (info.curve_limit == 90.0)		info.curve_limit = 0.0;
	else if (info.curve_limit ==180.0)		info.curve_limit =-1.0;
	else if (info.curve_limit ==  0.0)		info.curve_limit = 1.0;
	else									info.curve_limit = cos(info.curve_limit * DEG_TO_RAD);

	gBridgeInfo.push_back(info);
	return true;
}

void	LoadNetFeatureTables(void)
{
	gNetFeatures.clear();
	gNetReps.clear();
	gFeature2Rep.clear();
	gBridgeInfo.clear();
	gForkRules.clear();
	gChangeRules.clear();

	RegisterLineHandler("ROAD_GENERAL", RoadGeneralProps, NULL);
	RegisterLineHandler("ROAD_PROP", ReadRoadSpecificProps, NULL);
	RegisterLineHandler("ROAD_PICK", ReadRoadPick, NULL);
	RegisterLineHandler("FORK_RULE",ReadForkRule,NULL);
	RegisterLineHandler("CHANGE_RULE",ReadChangeRule,NULL);
	RegisterLineHandler("ROAD_BRIDGE", ReadRoadBridge, NULL);
	LoadConfigFile("road_properties.txt");
}

//bool	IsSeparatedHighway(int feat_type)
//{
//	if (gNetFeatures.count(feat_type) == 0) return false;
//	return gNetFeatures[feat_type].oneway_feature != NO_VALUE;
//}
//
//int		SeparatedToOneway(int feat_type)
//{
//	if (gNetFeatures.count(feat_type) == 0) return feat_type;
//	int new_type = gNetFeatures[feat_type].oneway_feature;
//	if (new_type == NO_VALUE) return feat_type;
//	return new_type;
//}
//

bool	IsOneway(int rep_type)
{
	if (gNetReps.count(rep_type) == 0) return 0;
	return gNetReps[rep_type].is_oneway;
}


int		FindBridgeRule(int rep_type, double len, double smallest_seg, double biggest_seg, int num_segments, double curve_dot, double agl1, double agl2)
{
	DebugAssert(len > 0.0);
	DebugAssert(smallest_seg > 0.0);
	DebugAssert(biggest_seg > 0.0);
	DebugAssert(smallest_seg <= biggest_seg);
	DebugAssert(num_segments > 0);
	DebugAssert(len >= biggest_seg);

	for (int n = 0; n < gBridgeInfo.size(); ++n)
	{
		BridgeInfo& rule = gBridgeInfo[n];
		if (rule.rep_type == rep_type &&
			(rule.curve_limit <= curve_dot) &&
			(rule.min_length == rule.max_length || (rule.min_length <= len && len <= rule.max_length)) &&
			(rule.min_start_agl == rule.max_start_agl || agl1 == -1.0 || (rule.min_start_agl <= agl1 && agl1 <= rule.max_start_agl)) &&
			(rule.min_start_agl == rule.max_start_agl || agl2 == -1.0 || (rule.min_start_agl <= agl2 && agl2 <= rule.max_start_agl)) &&
			(rule.min_seg_length == rule.max_seg_length || (rule.min_seg_length <= smallest_seg && biggest_seg <= rule.max_seg_length)) &&
			(rule.min_seg_count == rule.max_seg_count || (rule.min_seg_count <= num_segments && num_segments <= rule.max_seg_count))
		)
		{
			return n;
		}
	}
	return -1;
}