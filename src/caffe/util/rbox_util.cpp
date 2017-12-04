#include <algorithm>
#include <csignal>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <math.h>
#include <omp.h>

#include "boost/iterator/counting_iterator.hpp"

#include "caffe/util/rbox_util.hpp"
using namespace std;
namespace caffe{

template <typename Dtype>
void GetGroundTruthR(const Dtype* gt_data, const int num_gt, const int background_label_id, 
	map<int, vector<NormalizedRBox> >* all_gt_rboxes)
{
	all_gt_rboxes->clear();
	for (int i = 0; i < num_gt; ++i)
	{
		int start_idx = i * 7;
		int item_id = gt_data[start_idx];
		if (item_id == -1)
		{
			continue;
		}
		int label = gt_data[start_idx + 1];
		CHECK_NE(background_label_id, label)
			<< "Found background label in the dataset.";
		NormalizedRBox rbox;
		rbox.set_label(label);
		rbox.set_xcenter(gt_data[start_idx + 2]);
		rbox.set_ycenter(gt_data[start_idx + 3]);
		rbox.set_angle(gt_data[start_idx + 4]);
		rbox.set_width(gt_data[start_idx + 5]);
		rbox.set_height(gt_data[start_idx + 6]);
		(*all_gt_rboxes)[item_id].push_back(rbox);
		//LOG(INFO)<<rbox.xcenter()<<" "<<rbox.ycenter()<<" "<<rbox.width()<<" "<<rbox.height()<<" "<<rbox.angle();
	}
}

// Explicit initialization.
template void GetGroundTruthR(const float* gt_data, const int num_gt,
      const int background_label_id, map<int, vector<NormalizedRBox> >* all_gt_rboxes);
template void GetGroundTruthR(const double* gt_data, const int num_gt,
      const int background_label_id, map<int, vector<NormalizedRBox> >* all_gt_rboxes);

template <typename Dtype>
void GetPriorRBoxes(const Dtype* prior_data, const int num_priors,
	const bool regress_angle, const bool regress_size,
	const float width, const float height,
	vector<NormalizedRBox>* prior_rboxes,
	vector<vector<float> >* prior_variances) 
{
	prior_rboxes->clear();
	prior_variances->clear();
	int num_param = 2;
	if (regress_angle) num_param ++;
	if (regress_size) num_param += 2;
	
	for (int i = 0; i < num_priors; ++i)
	{
		int start_idx = i * num_param;
		NormalizedRBox rbox;
		int count = 0;
		rbox.set_xcenter(prior_data[start_idx + count]);
		count++;
		rbox.set_ycenter(prior_data[start_idx + count]);
		count++;
		if (regress_size)
		{
			rbox.set_width(prior_data[start_idx + count]);
			count++;
			rbox.set_height(prior_data[start_idx + count]);
			count++;
		}
		else
		{
			rbox.set_width(width);
			rbox.set_height(height);
		}
		if (regress_angle) rbox.set_angle(prior_data[start_idx + count]);
		else rbox.set_angle(0.0);
		prior_rboxes->push_back(rbox);
	}
	for (int i = 0; i < num_priors; ++i) 
	{
		int start_idx = (num_priors + i) * num_param;
		vector<float> var;
		for (int j = 0; j < num_param; ++j) 
		{
			var.push_back(prior_data[start_idx + j]);
		}
		prior_variances->push_back(var);
	}
}

// Explicit initialization.
template void GetPriorRBoxes(const float* prior_data, const int num_priors,const bool regress_angle, const bool regress_size,
const float width, const float height,
	vector<NormalizedRBox>* prior_rboxes,
	vector<vector<float> >* prior_variances);
template void GetPriorRBoxes(const double* prior_data, const int num_priors,const bool regress_angle, const bool regress_size, const float width, const float height,
	vector<NormalizedRBox>* prior_rboxes,
	vector<vector<float> >* prior_variances);

template <typename Dtype>
void GetLocPredictionsR(const Dtype* loc_data, const int num,
	const int num_preds,  const bool regress_angle, const bool regress_size,
	vector<LabelRBox>* loc_preds) 
{
	loc_preds->clear();
	loc_preds->resize(num);
	int num_param = 2;
	if (regress_angle) num_param ++;
	if (regress_size) num_param += 2;
	for (int i = 0; i < num; ++i)
	{
		LabelRBox& rbox = (*loc_preds)[i];
		for (int p = 0; p < num_preds; ++p)
		{
			int start_idx = p * num_param;
			int label = -1;
			if (rbox.find(label) == rbox.end()) 
				rbox[label].resize(num_preds);
			int count = 0;
			rbox[label][p].set_xcenter(loc_data[start_idx + count]);
			count ++;
			rbox[label][p].set_ycenter(loc_data[start_idx + count]);
			count ++;
			if (regress_size)
			{
				rbox[label][p].set_width(loc_data[start_idx + count]);
				count ++;
				rbox[label][p].set_height(loc_data[start_idx + count]);
				count ++;
			}
			else
			{
				rbox[label][p].set_width(-1);
				rbox[label][p].set_height(-1);
			}
			if (regress_angle)
				rbox[label][p].set_angle(loc_data[start_idx + count]);
			else
				rbox[label][p].set_angle(0);
		}
		loc_data += num_preds * num_param;
	}
}

// Explicit initialization.
template void GetLocPredictionsR(const float* loc_data, const int num,
		const int num_preds,  const bool regress_angle, const bool regress_size,
		vector<LabelRBox>* loc_preds);
template void GetLocPredictionsR(const double* loc_data, const int num,
		const int num_preds,  const bool regress_angle, const bool regress_size,
		vector<LabelRBox>* loc_preds);
	
template <typename Dtype>
void GetLocPredictionsR(const Dtype* loc_data, const int num,
	const int num_preds_per_class, const int num_loc_classes,
	const bool share_location, const bool regress_angle, const bool regress_size,
	vector<LabelRBox>* loc_preds)
{
	loc_preds->clear();
	if (share_location) {
		CHECK_EQ(num_loc_classes, 1);
	}
	loc_preds->resize(num);
	int num_param = 2;
	if (regress_angle) num_param ++;
	if (regress_size) num_param += 2;
	//const Dtype* loc_data1 = loc_data;
	#pragma omp parallel for
	for (int i = 0; i < num; ++i) {
		LabelRBox& label_rbox = (*loc_preds)[i];
		const Dtype* loc_data1 = loc_data + num_preds_per_class * num_loc_classes * num_param * i;
		for (int p = 0; p < num_preds_per_class; ++p) {
			int start_idx = p * num_loc_classes * num_param;
			for (int c = 0; c < num_loc_classes; ++c) {
				int label = share_location ? -1 : c;
				if (label_rbox.find(label) == label_rbox.end()) {
					label_rbox[label].resize(num_preds_per_class);
				}
				int count = 0;
				label_rbox[label][p].set_xcenter(loc_data1[start_idx + c * num_param + count]);
				count ++;
				label_rbox[label][p].set_ycenter(loc_data1[start_idx + c * num_param + count]);
				count ++;
				if (regress_size)
				{
					label_rbox[label][p].set_width(loc_data1[start_idx + c * num_param + count]);
					count ++;
					label_rbox[label][p].set_height(loc_data1[start_idx + c * num_param + count]);
					count ++;
				}
				else
				{
					label_rbox[label][p].set_width(-1);
					label_rbox[label][p].set_height(-1);
				}
				if (regress_angle)
					label_rbox[label][p].set_angle(loc_data1[start_idx + c * num_param + count]);
				else
					label_rbox[label][p].set_angle(0);
					
			}
		}
		
	}
}

// Explicit initialization.
template void GetLocPredictionsR(const float* loc_data, const int num,
	const int num_preds_per_class, const int num_loc_classes,
	const bool share_location, const bool regress_angle, const bool regress_size,
	vector<LabelRBox>* loc_preds);
template void GetLocPredictionsR(const double* loc_data, const int num,
	const int num_preds_per_class, const int num_loc_classes,
	const bool share_location, const bool regress_angle, const bool regress_size,
	vector<LabelRBox>* loc_preds);	
	
	
float OverlapArea (float width, float height, float xcenter1, float ycenter1, float xcenter2, float ycenter2, float angle1, float angle2)
{
	angle1 = -angle1;
	angle2 = -angle2;
	float angled = angle2 - angle1;
	angled *= (float)3.14159265/180;
	angle1 *= (float)3.14159265/180;
	//ofstream fout("Output.txt");
	float area = 0;
	float hw = width / 2;
	float hh = height /2;
	float xcenterd = xcenter2 - xcenter1;
	float ycenterd = ycenter2 - ycenter1;
	float tmp = xcenterd * cosf(angle1) + ycenterd * sinf(angle1);
	ycenterd = -xcenterd * sinf(angle1) + ycenterd * cosf(angle1);
	xcenterd = tmp;
	float max_width_height = width > height? width : height;
	//初步筛选掉距离过远的矩形
	if (sqrt(xcenterd * xcenterd + ycenterd * ycenterd) > max_width_height * 1.414214)
	{
		area = 0;
		//fout<<endl<<"AREA = 0"<<endl;
		//fout.close();
		return (area);
	}
// 判断angled是否是0、90、180、270度
	if (fabs(sin(angled)) < 1e-3)
	{
		if (fabs(xcenterd) > width || fabs(ycenterd) > height)
		{
			area = 0;
			//fout<<endl<<"AREA = 0"<<endl;
			//fout.close();
			return (area);
		}
		else
		{
			const float inter_width = width - fabs(xcenter2-xcenter1);
			const float inter_height = height - fabs(ycenter2-ycenter1);
			const float inter_size = inter_width * inter_height;
			area = inter_size;
			//fout<<endl<<"AREA = "<<area<<endl;
			//fout.close();
			return (area);
		}
	}
	if (fabs(cos(angled)) < 1e-3)
	{
		float wl = max(xcenterd - hh, -hw);
		float wr = min(xcenterd + hh, hw);
		float hd = max(ycenterd - hw, -hh);
		float hu = min(ycenterd + hw, hh);
		if (wl >= wr || hd >= hu) area = 0;
		else area = (wr - wl) * (hu - hd);
		//fout<<endl<<"AREA = "<<area<<endl;
		//fout.close();
		return (area);
	}
	
	float cos_angled = cosf(angled);
	float sin_angled = sinf(angled);
	float cos_angled_hw = cos_angled * hw;
	float sin_angled_hw = sin_angled * hw;
	float cos_angled_hh = cos_angled * hh;
	float sin_angled_hh = sin_angled * hh;
	
	// point20: (w/2, h/2)
	float point2x[4], point2y[4];
	point2x[0] = xcenterd + cos_angled_hw - sin_angled_hh;
	point2y[0] = ycenterd + sin_angled_hw + cos_angled_hh;
	// point21: (-w/2, h/2)
	point2x[1] = xcenterd - cos_angled_hw - sin_angled_hh;
	point2y[1] = ycenterd - sin_angled_hw + cos_angled_hh; 
	// point22: (-w/2, -h/2)
	point2x[2] = xcenterd - cos_angled_hw + sin_angled_hh;
	point2y[2] = ycenterd - sin_angled_hw - cos_angled_hh;
	// point23: (w/2, -h/2)
	point2x[3] = xcenterd + cos_angled_hw + sin_angled_hh;
	point2y[3] = ycenterd + sin_angled_hw - cos_angled_hh; 

	float pcenter_x = 0, pcenter_y = 0;
	int count = 0;

	// determine the inner point
	bool inner_side2[4][4], inner2[4];
	for(int i = 0; i < 4; i++)
	{
		inner_side2[i][0] = point2y[i] < hh;
		inner_side2[i][1] = point2x[i] > -hw;
		inner_side2[i][2] = point2y[i] > -hh;
		inner_side2[i][3] = point2x[i] < hw;
		inner2[i] = inner_side2[i][0] & inner_side2[i][1] & inner_side2[i][2] & inner_side2[i][3];
		if (inner2[i]) { pcenter_x += point2x[i]; pcenter_y += point2y[i]; count++;}
	}

	//similar operating for rbox1: angled -> -angled, xcenterd -> -xcenterd, ycenterd -> -ycenterd
	// point10: (w/2, h/2)
	float xcenterd_hat = - xcenterd * cos_angled - ycenterd * sin_angled;
	float ycenterd_hat = xcenterd * sin_angled - ycenterd * cos_angled;
	float point1x[4], point1y[4];
	
	point1x[0] = xcenterd_hat + cos_angled_hw + sin_angled_hh;
	point1y[0] = ycenterd_hat - sin_angled_hw + cos_angled_hh;
	// point21: (-w/2, h/2)
	point1x[1] = xcenterd_hat - cos_angled_hw + sin_angled_hh;
	point1y[1] = ycenterd_hat + sin_angled_hw + cos_angled_hh; 
	// point22: (-w/2, -h/2)
	point1x[2] = xcenterd_hat - cos_angled_hw - sin_angled_hh;
	point1y[2] = ycenterd_hat + sin_angled_hw - cos_angled_hh;
	// point23: (w/2, -h/2)
	point1x[3] = xcenterd_hat + cos_angled_hw - sin_angled_hh;
	point1y[3] = ycenterd_hat - sin_angled_hw - cos_angled_hh;
	
	// determine the inner point
	// determine the inner point
	bool inner_side1[4][4], inner1[4];
	for(int i = 0; i < 4; i++)
	{
		inner_side1[i][0] = point1y[i] < hh;
		inner_side1[i][1] = point1x[i] > -hw;
		inner_side1[i][2] = point1y[i] > -hh;
		inner_side1[i][3] = point1x[i] < hw;
		inner1[i] = inner_side1[i][0] & inner_side1[i][1] & inner_side1[i][2] & inner_side1[i][3];
	}
	point1x[0] = hw;
	point1y[0] = hh;
	// point21: (-w/2, h/2)
	point1x[1] = -hw;
	point1y[1] = hh;
	// point22: (-w/2, -h/2)
	point1x[2] = -hw;
	point1y[2] = -hh;
	// point23: (w/2, -h/2)
	point1x[3] = hw;
	point1y[3] = -hh;
	if (inner1[0]) { pcenter_x += hw; pcenter_y += hh; count++;}
	if (inner1[1]) { pcenter_x -= hw; pcenter_y += hh; count++;}
	if (inner1[2]) { pcenter_x -= hw; pcenter_y -= hh; count++;}
	if (inner1[3]) { pcenter_x += hw; pcenter_y -= hh; count++;}
	//find cross_points
	Line line1[4], line2[4];
	line1[0].p1 = 0; line1[0].p2 = 1;
	line1[1].p1 = 1; line1[1].p2 = 2;
	line1[2].p1 = 2; line1[2].p2 = 3;
	line1[3].p1 = 3; line1[3].p2 = 0;
	line2[0].p1 = 0; line2[0].p2 = 1;
	line2[1].p1 = 1; line2[1].p2 = 2;
	line2[2].p1 = 2; line2[2].p2 = 3;
	line2[3].p1 = 3; line2[3].p2 = 0;
	float pointc_x[4][4], pointc_y[4][4];
	for (int i = 0; i < 4; i++)
	{
		int index1 = line1[i].p1;
		int index2 = line1[i].p2;
		line1[i].crossnum = 0;
		if (inner1[index1] && inner1[index2])
		{
			if (i == 0 || i == 2) line1[i].length = width;
			else line1[i].length = height;
			line1[i].crossnum = -1;
			continue;
		}
		if (inner1[index1])
		{
			line1[i].crossnum ++;
			line1[i].d[0][0] = index1;
			line1[i].d[0][1] = -1;
			continue;
		}
		if (inner1[index2])
		{
			line1[i].crossnum ++;
			line1[i].d[0][0] = index2;
			line1[i].d[0][1] = -1;
			continue;
		}
	}
	for (int i = 0; i < 4; i++)
	{
		int index1 = line2[i].p1;
		float x1 = point2x[index1];
		float y1 = point2y[index1];
		int index2 = line2[i].p2;
		float x2 = point2x[index2];
		float y2 = point2y[index2];
		line2[i].crossnum = 0;
		if (inner2[index1] && inner2[index2])
		{
			if (i == 0 || i == 2) line2[i].length = width;
			else line2[i].length = height;
			line2[i].crossnum = -1;
			continue;
		}
		if (inner2[index1])
		{
			line2[i].crossnum ++;
			line2[i].d[0][0] = index1;
			line2[i].d[0][1] = -1;
		}
		else if (inner2[index2])
		{
			line2[i].crossnum ++;
			line2[i].d[0][0] = index2;
			line2[i].d[0][1] = -1;
		}
		float tmp1 = (y1*x2 - y2*x1) / (y1 - y2);
		float tmp2 = (x1 - x2) / (y1 - y2);
		float tmp3 = (x1*y2 - x2*y1) / (x1 - x2);
		float tmp4 = 1/tmp2 * hw;
		tmp2 *= hh;
		for (int j = 0; j < 4; j++)
		{
			int index3 = line1[j].p1;
			int index4 = line1[j].p2;
			if ((inner_side2[index1][j] != inner_side2[index2][j]) 
				&& (inner_side1[index3][i] != inner_side1[index4][i]))
			{
				//计算交点
				switch (j)
				{
				case 0: 
					pointc_x[i][j] = tmp1 + tmp2;
					pointc_y[i][j] = hh;
					break;
				case 1:
					pointc_y[i][j] = tmp3 - tmp4;
					pointc_x[i][j] = -hw;
					break;
				case 2:
					pointc_x[i][j] = tmp1 - tmp2;
					pointc_y[i][j] = -hh;
					break;
				case 3:
					pointc_y[i][j] = tmp3 + tmp4;
					pointc_x[i][j] = hw;
					break;
				default:
					break;
				}
				line1[j].d[line1[j].crossnum][0] = i;
				line1[j].d[line1[j].crossnum ++][1] = j;
				line2[i].d[line2[i].crossnum][0] = i;
				line2[i].d[line2[i].crossnum ++][1] = j;
				pcenter_x += pointc_x[i][j];
				pcenter_y += pointc_y[i][j];
				count ++;
			}
		}
	}
	pcenter_x /= (float)count;
	pcenter_y /= (float)count;
	//计算以矩阵2为参照系时的坐标
	float pcenter_x_hat, pcenter_y_hat;
	pcenter_x_hat = pcenter_x - xcenterd;
	pcenter_y_hat = pcenter_y - ycenterd;
	tmp = cos_angled * pcenter_x_hat + sin_angled * pcenter_y_hat;
	pcenter_y_hat = -sin_angled * pcenter_x_hat + cos_angled * pcenter_y_hat;
	pcenter_x_hat = tmp;
	//计算多边形边长
	for (int i = 0; i < 4; i++)
	{
		if (line1[i].crossnum > 0)
		{
			if (line1[i].d[0][1] == -1)
			{
				if (i==0 || i==2) 
					line1[i].length = fabs(point1x[line1[i].d[0][0]] - pointc_x[line1[i].d[1][0]][line1[i].d[1][1]]);
				else
					line1[i].length = fabs(point1y[line1[i].d[0][0]] - pointc_y[line1[i].d[1][0]][line1[i].d[1][1]]);
			}
			else
			{
				if (i==0 || i==2)
					line1[i].length = fabs(pointc_x[line1[i].d[0][0]][line1[i].d[0][1]] - pointc_x[line1[i].d[1][0]][line1[i].d[1][1]]);
				else
					line1[i].length = fabs(pointc_y[line1[i].d[0][0]][line1[i].d[0][1]] - pointc_y[line1[i].d[1][0]][line1[i].d[1][1]]);
			}
		}
		if (line2[i].crossnum >0)
		{
			if (line2[i].d[0][1] == -1)
				line2[i].length = fabs(point2x[line2[i].d[0][0]] - pointc_x[line2[i].d[1][0]][line2[i].d[1][1]]);
			else
				line2[i].length = fabs(pointc_x[line2[i].d[0][0]][line2[i].d[0][1]] - pointc_x[line2[i].d[1][0]][line2[i].d[1][1]]);
			if(i == 0 || i == 2) line2[i].length *= width / fabs(point2x[line2[i].p1] - point2x[line2[i].p2]);
			else line2[i].length *= height / fabs(point2x[line2[i].p1] - point2x[line2[i].p2]);
		}
	}

	//计算面积
	float dis1[4], dis2[4];
	dis1[0] = fabs(pcenter_y - hh);
	dis1[1] = fabs(pcenter_x + hw);
	dis1[2] = fabs(pcenter_y + hh);
	dis1[3] = fabs(pcenter_x - hw);
	dis2[0] = fabs(pcenter_y_hat - hh);
	dis2[1] = fabs(pcenter_x_hat + hw);
	dis2[2] = fabs(pcenter_y_hat + hh);
	dis2[3] = fabs(pcenter_x_hat - hw);
	for (int i=0; i < 4; i++)
	{
		if (line1[i].crossnum != 0)
			area += dis1[i] * line1[i].length;
		if (line2[i].crossnum != 0)
			area += dis2[i] * line2[i].length;
	}
	area /= 2;
	return (area);
}

float OverlapArea (float width1, float height1, float width2, float height2, 
	float xcenter1,	float ycenter1, float xcenter2, float ycenter2, 
	float angle1, float angle2)
{
	angle1 = -angle1;
	angle2 = -angle2;
	float angled = angle2 - angle1;
	angled *= (float)3.14159265/180;
	angle1 *= (float)3.14159265/180;
	//ofstream fout("Output.txt");
	float area = 0;
	float hw1 = width1 / 2;
	float hh1 = height1 /2;
	float hw2 = width2 / 2;
	float hh2 = height2 /2;
	float xcenterd = xcenter2 - xcenter1;
	float ycenterd = ycenter2 - ycenter1;
	float tmp = xcenterd * cosf(angle1) + ycenterd * sinf(angle1);
	ycenterd = -xcenterd * sinf(angle1) + ycenterd * cosf(angle1);
	xcenterd = tmp;
	float max_width_height1 = width1 > height1? width1 : height1;
	float max_width_height2 = width2 > height2? width2 : height2;
	if (sqrt(xcenterd * xcenterd + ycenterd * ycenterd) > 
		(max_width_height1 + max_width_height2) * 1.414214/2)
	{
		area = 0;
		//fout<<endl<<"AREA = 0"<<endl;
		//fout.close();
		return (area);
	}
	if (fabs(sin(angled)) < 1e-3)
	{
		if (fabs(xcenterd) > (hw1 + hw2) || fabs(ycenterd) > (hh1 + hh2))
		{
			area = 0;
			//fout<<endl<<"AREA = 0"<<endl;
			//fout.close();
			return (area);
		}
		else
		{
			float x_min_inter = -hw1 > (xcenterd - hw2)? -hw1 : (xcenterd - hw2);
			float x_max_inter = hw1 < (xcenterd + hw2)? hw1 : (xcenterd + hw2);
			float y_min_inter = -hh1 > (ycenterd - hh2)? -hh1 : (ycenterd - hh2);
			float y_max_inter = hh1 < (ycenterd + hh2)? hh1 : (ycenterd + hh2);
			const float inter_width = x_max_inter - x_min_inter;
			const float inter_height = y_max_inter - y_min_inter;
			const float inter_size = inter_width * inter_height;
			area = inter_size;
			//LOG(INFO)<<"AREA = "<<area;
			//fout.close();
			return (area);
		}
	}
	if (fabs(cos(angled)) < 1e-3)
	{
		float x_min_inter = -hw1 > (xcenterd - hh2)? -hw1 : (xcenterd - hh2);
		float x_max_inter = hw1 < (xcenterd + hh2)? hw1 : (xcenterd + hh2);
		float y_min_inter = -hh1 > (ycenterd - hw2)? -hh1 : (ycenterd - hw2);
		float y_max_inter = hh1 < (ycenterd + hw2)? hh1 : (ycenterd + hw2);
		const float inter_width = x_max_inter - x_min_inter;
		const float inter_height = y_max_inter - y_min_inter;
		const float inter_size = inter_width * inter_height;
		area = inter_size;
		//fout<<endl<<"AREA = "<<area<<endl;
		//fout.close();
		return (area);
	}
	
	float cos_angled = cosf(angled);
	float sin_angled = sinf(angled);
	float cos_angled_hw1 = cos_angled * hw1;
	float sin_angled_hw1 = sin_angled * hw1;
	float cos_angled_hh1 = cos_angled * hh1;
	float sin_angled_hh1 = sin_angled * hh1;
	float cos_angled_hw2 = cos_angled * hw2;
	float sin_angled_hw2 = sin_angled * hw2;
	float cos_angled_hh2 = cos_angled * hh2;
	float sin_angled_hh2 = sin_angled * hh2;
	
	// point20: (w/2, h/2)
	float point2x[4], point2y[4];
	point2x[0] = xcenterd + cos_angled_hw2 - sin_angled_hh2;
	point2y[0] = ycenterd + sin_angled_hw2 + cos_angled_hh2;
	// point21: (-w/2, h/2)
	point2x[1] = xcenterd - cos_angled_hw2 - sin_angled_hh2;
	point2y[1] = ycenterd - sin_angled_hw2 + cos_angled_hh2; 
	// point22: (-w/2, -h/2)
	point2x[2] = xcenterd - cos_angled_hw2 + sin_angled_hh2;
	point2y[2] = ycenterd - sin_angled_hw2 - cos_angled_hh2;
	// point23: (w/2, -h/2)
	point2x[3] = xcenterd + cos_angled_hw2 + sin_angled_hh2;
	point2y[3] = ycenterd + sin_angled_hw2 - cos_angled_hh2; 

	float pcenter_x = 0, pcenter_y = 0;
	int count = 0;

	// determine the inner point
	bool inner_side2[4][4], inner2[4];
	for(int i = 0; i < 4; i++)
	{
		inner_side2[i][0] = point2y[i] < hh1;
		inner_side2[i][1] = point2x[i] > -hw1;
		inner_side2[i][2] = point2y[i] > -hh1;
		inner_side2[i][3] = point2x[i] < hw1;
		inner2[i] = inner_side2[i][0] & inner_side2[i][1] & inner_side2[i][2] & inner_side2[i][3];
		if (inner2[i]) { pcenter_x += point2x[i]; pcenter_y += point2y[i]; count++;}
	}

	//similar operating for rbox1: angled -> -angled, xcenterd -> -xcenterd, ycenterd -> -ycenterd
	// point10: (w/2, h/2)
	float xcenterd_hat = - xcenterd * cos_angled - ycenterd * sin_angled;
	float ycenterd_hat = xcenterd * sin_angled - ycenterd * cos_angled;
	float point1x[4], point1y[4];
	
	point1x[0] = xcenterd_hat + cos_angled_hw1 + sin_angled_hh1;
	point1y[0] = ycenterd_hat - sin_angled_hw1 + cos_angled_hh1;
	// point21: (-w/2, h/2)
	point1x[1] = xcenterd_hat - cos_angled_hw1 + sin_angled_hh1;
	point1y[1] = ycenterd_hat + sin_angled_hw1 + cos_angled_hh1; 
	// point22: (-w/2, -h/2)
	point1x[2] = xcenterd_hat - cos_angled_hw1 - sin_angled_hh1;
	point1y[2] = ycenterd_hat + sin_angled_hw1 - cos_angled_hh1;
	// point23: (w/2, -h/2)
	point1x[3] = xcenterd_hat + cos_angled_hw1 - sin_angled_hh1;
	point1y[3] = ycenterd_hat - sin_angled_hw1 - cos_angled_hh1;
	
	// determine the inner point
	// determine the inner point
	bool inner_side1[4][4], inner1[4];
	for(int i = 0; i < 4; i++)
	{
		inner_side1[i][0] = point1y[i] < hh2;
		inner_side1[i][1] = point1x[i] > -hw2;
		inner_side1[i][2] = point1y[i] > -hh2;
		inner_side1[i][3] = point1x[i] < hw2;
		inner1[i] = inner_side1[i][0] & inner_side1[i][1] & inner_side1[i][2] & inner_side1[i][3];
	}
	point1x[0] = hw1;
	point1y[0] = hh1;
	// point21: (-w/2, h/2)
	point1x[1] = -hw1;
	point1y[1] = hh1;
	// point22: (-w/2, -h/2)
	point1x[2] = -hw1;
	point1y[2] = -hh1;
	// point23: (w/2, -h/2)
	point1x[3] = hw1;
	point1y[3] = -hh1;
	if (inner1[0]) { pcenter_x += hw1; pcenter_y += hh1; count++;}
	if (inner1[1]) { pcenter_x -= hw1; pcenter_y += hh1; count++;}
	if (inner1[2]) { pcenter_x -= hw1; pcenter_y -= hh1; count++;}
	if (inner1[3]) { pcenter_x += hw1; pcenter_y -= hh1; count++;}
	//find cross_points
	Line line1[4], line2[4];
	line1[0].p1 = 0; line1[0].p2 = 1;
	line1[1].p1 = 1; line1[1].p2 = 2;
	line1[2].p1 = 2; line1[2].p2 = 3;
	line1[3].p1 = 3; line1[3].p2 = 0;
	line2[0].p1 = 0; line2[0].p2 = 1;
	line2[1].p1 = 1; line2[1].p2 = 2;
	line2[2].p1 = 2; line2[2].p2 = 3;
	line2[3].p1 = 3; line2[3].p2 = 0;
	float pointc_x[4][4], pointc_y[4][4];
	for (int i = 0; i < 4; i++)
	{
		int index1 = line1[i].p1;
		int index2 = line1[i].p2;
		line1[i].crossnum = 0;
		if (inner1[index1] && inner1[index2])
		{
			if (i == 0 || i == 2) line1[i].length = width1;
			else line1[i].length = height1;
			line1[i].crossnum = -1;
			continue;
		}
		if (inner1[index1])
		{
			line1[i].crossnum ++;
			line1[i].d[0][0] = index1;
			line1[i].d[0][1] = -1;
			continue;
		}
		if (inner1[index2])
		{
			line1[i].crossnum ++;
			line1[i].d[0][0] = index2;
			line1[i].d[0][1] = -1;
			continue;
		}
	}
	for (int i = 0; i < 4; i++)
	{
		int index1 = line2[i].p1;
		float x1 = point2x[index1];
		float y1 = point2y[index1];
		int index2 = line2[i].p2;
		float x2 = point2x[index2];
		float y2 = point2y[index2];
		line2[i].crossnum = 0;
		if (inner2[index1] && inner2[index2])
		{
			if (i == 0 || i == 2) line2[i].length = width2;
			else line2[i].length = height1;
			line2[i].crossnum = -1;
			continue;
		}
		if (inner2[index1])
		{
			line2[i].crossnum ++;
			line2[i].d[0][0] = index1;
			line2[i].d[0][1] = -1;
		}
		else if (inner2[index2])
		{
			line2[i].crossnum ++;
			line2[i].d[0][0] = index2;
			line2[i].d[0][1] = -1;
		}
		float tmp1 = (y1*x2 - y2*x1) / (y1 - y2);
		float tmp2 = (x1 - x2) / (y1 - y2);
		float tmp3 = (x1*y2 - x2*y1) / (x1 - x2);
		float tmp4 = 1/tmp2 * hw1;
		tmp2 *= hh1;
		for (int j = 0; j < 4; j++)
		{
			int index3 = line1[j].p1;
			int index4 = line1[j].p2;
			if ((inner_side2[index1][j] != inner_side2[index2][j]) 
				&& (inner_side1[index3][i] != inner_side1[index4][i]))
			{
				switch (j)
				{
				case 0: 
					pointc_x[i][j] = tmp1 + tmp2;
					pointc_y[i][j] = hh1;
					break;
				case 1:
					pointc_y[i][j] = tmp3 - tmp4;
					pointc_x[i][j] = -hw1;
					break;
				case 2:
					pointc_x[i][j] = tmp1 - tmp2;
					pointc_y[i][j] = -hh1;
					break;
				case 3:
					pointc_y[i][j] = tmp3 + tmp4;
					pointc_x[i][j] = hw1;
					break;
				default:
					break;
				}
				line1[j].d[line1[j].crossnum][0] = i;
				line1[j].d[line1[j].crossnum ++][1] = j;
				line2[i].d[line2[i].crossnum][0] = i;
				line2[i].d[line2[i].crossnum ++][1] = j;
				pcenter_x += pointc_x[i][j];
				pcenter_y += pointc_y[i][j];
				count ++;
			}
		}
	}
	pcenter_x /= (float)count;
	pcenter_y /= (float)count;
	float pcenter_x_hat, pcenter_y_hat;
	pcenter_x_hat = pcenter_x - xcenterd;
	pcenter_y_hat = pcenter_y - ycenterd;
	tmp = cos_angled * pcenter_x_hat + sin_angled * pcenter_y_hat;
	pcenter_y_hat = -sin_angled * pcenter_x_hat + cos_angled * pcenter_y_hat;
	pcenter_x_hat = tmp;

	for (int i = 0; i < 4; i++)
	{
		if (line1[i].crossnum > 0)
		{
			if (line1[i].d[0][1] == -1)
			{
				if (i==0 || i==2) 
					line1[i].length = fabs(point1x[line1[i].d[0][0]] - pointc_x[line1[i].d[1][0]][line1[i].d[1][1]]);
				else
					line1[i].length = fabs(point1y[line1[i].d[0][0]] - pointc_y[line1[i].d[1][0]][line1[i].d[1][1]]);
			}
			else
			{
				if (i==0 || i==2)
					line1[i].length = fabs(pointc_x[line1[i].d[0][0]][line1[i].d[0][1]] - pointc_x[line1[i].d[1][0]][line1[i].d[1][1]]);
				else
					line1[i].length = fabs(pointc_y[line1[i].d[0][0]][line1[i].d[0][1]] - pointc_y[line1[i].d[1][0]][line1[i].d[1][1]]);
			}
		}
		if (line2[i].crossnum >0)
		{
			if (line2[i].d[0][1] == -1)
				line2[i].length = fabs(point2x[line2[i].d[0][0]] - pointc_x[line2[i].d[1][0]][line2[i].d[1][1]]);
			else
				line2[i].length = fabs(pointc_x[line2[i].d[0][0]][line2[i].d[0][1]] - pointc_x[line2[i].d[1][0]][line2[i].d[1][1]]);
			if(i == 0 || i == 2) line2[i].length *= width2 / fabs(point2x[line2[i].p1] - point2x[line2[i].p2]);
			else line2[i].length *= height2 / fabs(point2x[line2[i].p1] - point2x[line2[i].p2]);
		}
	}

	float dis1[4], dis2[4];
	dis1[0] = fabs(pcenter_y - hh1);
	dis1[1] = fabs(pcenter_x + hw1);
	dis1[2] = fabs(pcenter_y + hh1);
	dis1[3] = fabs(pcenter_x - hw1);
	dis2[0] = fabs(pcenter_y_hat - hh2);
	dis2[1] = fabs(pcenter_x_hat + hw2);
	dis2[2] = fabs(pcenter_y_hat + hh2);
	dis2[3] = fabs(pcenter_x_hat - hw2);
	for (int i=0; i < 4; i++)
	{
		if (line1[i].crossnum != 0)
			area += dis1[i] * line1[i].length;
		if (line2[i].crossnum != 0)
			area += dis2[i] * line2[i].length;
	}
	area /= 2;
	return (area);
}

float JaccardOverlapR(const NormalizedRBox& rbox1, const NormalizedRBox& rbox2,
	const float width, const float height)
{
	/*******************Old method*************************************************
	float area = OverlapArea(width, height, rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), rbox1.angle(), rbox2.angle());
	return area / (width * height * 2 - area) * fabs(cos((rbox1.angle()-rbox2.angle()) / 180 * 3.141593));
	*******************************************************************************/
	float area = OverlapArea(width, height, width, height, rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), rbox2.angle(), rbox2.angle());
	return area / (width * height * 2 - area) * cosf((rbox1.angle()-rbox2.angle()) / 180 * 3.141593);
}

float JaccardOverlapRR(const NormalizedRBox& rbox1, const NormalizedRBox& rbox2,
	const float width, const float height)
{
	float area = OverlapArea(width, height, width, height, rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), rbox1.angle(), rbox2.angle());
	return area / (width * height * 2 - area);
}

float JaccardOverlapR(const NormalizedRBox& rbox1, const NormalizedRBox& rbox2)
{
	/*******************Old method*************************************************
	float area = OverlapArea(width, height, rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), rbox1.angle(), rbox2.angle());
	return area / (width * height * 2 - area) * fabs(cos((rbox1.angle()-rbox2.angle()) / 180 * 3.141593));
	*******************************************************************************/
	float area = OverlapArea(rbox1.width(), rbox1.height(), rbox2.width(), rbox2.height(),
		rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), 
		rbox2.angle(), rbox2.angle());
	return area / (rbox1.width() * rbox1.height() + rbox2.width() * rbox2.height() - area) 
		* fabs((cosf((rbox1.angle()-rbox2.angle()) / 180 * 3.141593)));
}

float JaccardOverlapRR(const NormalizedRBox& rbox1, const NormalizedRBox& rbox2)
{
	float area = OverlapArea(rbox1.width(), rbox1.height(), rbox2.width(), rbox2.height(),
		rbox1.xcenter(), rbox1.ycenter(), rbox2.xcenter(), rbox2.ycenter(), 
		rbox1.angle(), rbox2.angle());
	return area / (rbox1.width() * rbox1.height() + rbox2.width() * rbox2.height() - area);
}


template <typename Dtype>
Dtype JaccardOverlapR(const Dtype* rbox1, const Dtype* rbox2)
{
	float height, width; //In this algorithm we suggest the size of the rbox is known, not to be optimized
	if (rbox1[3] < 0) width = rbox2[3];
	else width = rbox1[3];
	if (rbox1[4] < 0) height = rbox2[4];
	else height = rbox1[4];
	if (width < 0 || height <0) return Dtype(0.);
	if (fabs(rbox2[0]-rbox1[0]) > width || fabs(rbox2[1]-rbox1[1]) > height)
	{
		return Dtype(0.);
	}
	else
	{
		const Dtype inter_width = width - fabs(rbox2[0]-rbox1[0]);
		const Dtype inter_height = height - fabs(rbox2[1]-rbox1[1]);
		const Dtype inter_size = inter_width * inter_height;
		return inter_size / (width * height * 2 - inter_size) * fabs(cos((rbox1[2]-rbox2[2]) / 180 * 3.141593));
	}
}

template float JaccardOverlapR(const float* rbox1, const float* rbox2);
template double JaccardOverlapR(const double* rbox1, const double* rbox2);

void MatchRBox(const vector<NormalizedRBox>& gt_rboxes,
	const vector<NormalizedRBox>& pred_rboxes, const int label,
	const MatchType match_type, const float overlap_threshold,
	const bool ignore_cross_boundary_rbox,
	vector<int>* match_indices, vector<float>* match_overlaps) 
{
	int num_pred = pred_rboxes.size();
	match_indices->clear();
	match_indices->resize(num_pred, -1);
	match_overlaps->clear();
	match_overlaps->resize(num_pred, 0.);

	int num_gt = 0;
	vector<int> gt_indices;
	num_gt = gt_rboxes.size();
	for (int i = 0; i < num_gt; ++i)
		gt_indices.push_back(i);
	if (num_gt == 0)
		return;

	// Store the positive overlap between predictions and ground truth.
	map<int, map<int, float> > overlaps;
	for (int i = 0; i < num_pred; ++i) 
	{
		for (int j = 0; j < num_gt; ++j)
		{
			float overlap = JaccardOverlapR(pred_rboxes[i], gt_rboxes[gt_indices[j]]);
			if (overlap > 1e-6)
			{
				(*match_overlaps)[i] = std::max((*match_overlaps)[i], overlap);
				overlaps[i][j] = overlap;
			}
		}
	}

	// Bipartite matching.
	vector<int> gt_pool;
	for (int i = 0; i < num_gt; ++i)
		gt_pool.push_back(i);
	while (gt_pool.size() > 0)
	{
		// Find the most overlapped gt and cooresponding predictions.
		int max_idx = -1;
		int max_gt_idx = -1;
		float max_overlap = -1;
		for (map<int, map<int, float> >::iterator it = overlaps.begin();
			it != overlaps.end(); ++it)
		{
			int i = it->first;
			// The prediction already has matched ground truth or is ignored.    
			if ((*match_indices)[i] != -1) continue;  
			for (int p = 0; p < gt_pool.size(); ++p)
			{
				int j = gt_pool[p];
				// No overlap between the i-th prediction and j-th ground truth.
				if (it->second.find(j) == it->second.end()) continue;
				// Find the maximum overlapped pair.
				if (it->second[j] > max_overlap)
				{
					// If the prediction has not been matched to any ground truth,
					// and the overlap is larger than maximum overlap, update.
					max_idx = i;
					max_gt_idx = j;
					max_overlap = it->second[j];
				}
			}
		}
		if (max_idx == -1)
		{
			// Cannot find good match.
			break;
		}
		else
		{
			CHECK_EQ((*match_indices)[max_idx], -1);
			(*match_indices)[max_idx] = gt_indices[max_gt_idx];
			(*match_overlaps)[max_idx] = max_overlap;
			// Erase the ground truth.
			gt_pool.erase(std::find(gt_pool.begin(), gt_pool.end(), max_gt_idx));
		}
	}

	switch (match_type)
	{
		case MultiRBoxLossParameter_MatchType_BIPARTITE:
			// Already done.
			break;
		case MultiRBoxLossParameter_MatchType_PER_PREDICTION:
			// Get most overlaped for the rest prediction rboxes.
			for (map<int, map<int, float> >::iterator it = overlaps.begin();
			it != overlaps.end(); ++it)
			{
				int i = it->first;
				if ((*match_indices)[i] != -1)
				{
					// The prediction already has matched ground truth or is ignored.
					continue;
				}
				int max_gt_idx = -1;
				float max_overlap = -1;
				for (int j = 0; j < num_gt; ++j)
				{
					if (it->second.find(j) == it->second.end())
					{
						// No overlap between the i-th prediction and j-th ground truth.
						continue;
					}
					// Find the maximum overlapped pair.
					float overlap = it->second[j];
					if (overlap >= overlap_threshold && overlap > max_overlap)
					{
						// If the prediction has not been matched to any ground truth,
						// and the overlap is larger than maximum overlap, update.
						max_gt_idx = j;
						max_overlap = overlap;
					}
				}
				if (max_gt_idx != -1)
				{
					// Found a matched ground truth.
					CHECK_EQ((*match_indices)[i], -1);
					(*match_indices)[i] = gt_indices[max_gt_idx];
					(*match_overlaps)[i] = max_overlap;
				}
			}
			break;
		default:
			LOG(FATAL) << "Unknown matching type.";
			break;
	}
	return;
}

void MatchRBox(const vector<NormalizedRBox>& gt_rboxes,
	const vector<NormalizedRBox>& pred_rboxes, const int label,
	const MatchType match_type, const float overlap_threshold,
	const bool ignore_cross_boundary_rbox,
	vector<int>* match_indices, vector<float>* match_overlaps,
	const float prior_width, const float prior_height) 
{
	int num_pred = pred_rboxes.size();
	match_indices->clear();
	match_indices->resize(num_pred, -1);
	match_overlaps->clear();
	match_overlaps->resize(num_pred, 0.);

	int num_gt = 0;
	vector<int> gt_indices;
	num_gt = gt_rboxes.size();
	for (int i = 0; i < num_gt; ++i)
		gt_indices.push_back(i);
	if (num_gt == 0)
		return;

	// Store the positive overlap between predictions and ground truth.
	map<int, map<int, float> > overlaps;
	for (int i = 0; i < num_pred; ++i) 
	{
		for (int j = 0; j < num_gt; ++j)
		{
			float overlap = JaccardOverlapR(pred_rboxes[i], gt_rboxes[gt_indices[j]], 
				prior_width, prior_height);
			if (overlap > 1e-6)
			{
				(*match_overlaps)[i] = std::max((*match_overlaps)[i], overlap);
				overlaps[i][j] = overlap;
			}
		}
	}

	// Bipartite matching.
	vector<int> gt_pool;
	for (int i = 0; i < num_gt; ++i)
		gt_pool.push_back(i);
	while (gt_pool.size() > 0)
	{
		// Find the most overlapped gt and cooresponding predictions.
		int max_idx = -1;
		int max_gt_idx = -1;
		float max_overlap = -1;
		for (map<int, map<int, float> >::iterator it = overlaps.begin();
			it != overlaps.end(); ++it)
		{
			int i = it->first;
			// The prediction already has matched ground truth or is ignored.    
			if ((*match_indices)[i] != -1) continue;  
			for (int p = 0; p < gt_pool.size(); ++p)
			{
				int j = gt_pool[p];
				// No overlap between the i-th prediction and j-th ground truth.
				if (it->second.find(j) == it->second.end()) continue;
				// Find the maximum overlapped pair.
				if (it->second[j] > max_overlap)
				{
					// If the prediction has not been matched to any ground truth,
					// and the overlap is larger than maximum overlap, update.
					max_idx = i;
					max_gt_idx = j;
					max_overlap = it->second[j];
				}
			}
		}
		if (max_idx == -1)
		{
			// Cannot find good match.
			break;
		}
		else
		{
			CHECK_EQ((*match_indices)[max_idx], -1);
			(*match_indices)[max_idx] = gt_indices[max_gt_idx];
			(*match_overlaps)[max_idx] = max_overlap;
			// Erase the ground truth.
			gt_pool.erase(std::find(gt_pool.begin(), gt_pool.end(), max_gt_idx));
		}
	}

	switch (match_type)
	{
		case MultiRBoxLossParameter_MatchType_BIPARTITE:
			// Already done.
			break;
		case MultiRBoxLossParameter_MatchType_PER_PREDICTION:
			// Get most overlaped for the rest prediction rboxes.
			for (map<int, map<int, float> >::iterator it = overlaps.begin();
			it != overlaps.end(); ++it)
			{
				int i = it->first;
				if ((*match_indices)[i] != -1)
				{
					// The prediction already has matched ground truth or is ignored.
					continue;
				}
				int max_gt_idx = -1;
				float max_overlap = -1;
				for (int j = 0; j < num_gt; ++j)
				{
					if (it->second.find(j) == it->second.end())
					{
						// No overlap between the i-th prediction and j-th ground truth.
						continue;
					}
					// Find the maximum overlapped pair.
					float overlap = it->second[j];
					if (overlap >= overlap_threshold && overlap > max_overlap)
					{
						// If the prediction has not been matched to any ground truth,
						// and the overlap is larger than maximum overlap, update.
						max_gt_idx = j;
						max_overlap = overlap;
					}
				}
				if (max_gt_idx != -1)
				{
					// Found a matched ground truth.
					CHECK_EQ((*match_indices)[i], -1);
					(*match_indices)[i] = gt_indices[max_gt_idx];
					(*match_overlaps)[i] = max_overlap;
				}
			}
			break;
		default:
			LOG(FATAL) << "Unknown matching type.";
			break;
	}
	return;
}

void FindMatchesR(const vector<LabelRBox>& all_loc_preds,
		const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
		const vector<NormalizedRBox>& prior_rboxes,
		const vector<vector<float> >& prior_variances,
		const MultiRBoxLossParameter& multirbox_loss_param,
		vector<map<int, vector<float> > >* all_match_overlaps,
		vector<map<int, vector<int> > >* all_match_indices)
{
	// all_match_overlaps->clear();
	// all_match_indices->clear();
	// Get parameters.
	CHECK(multirbox_loss_param.has_num_classes()) << "Must provide num_classes.";
	const int num_classes = multirbox_loss_param.num_classes();
	CHECK_GE(num_classes, 1) << "num_classes should not be less than 1.";
	
	const bool share_location = multirbox_loss_param.share_location();
	const MatchType match_type = multirbox_loss_param.match_type();
	const float overlap_threshold = multirbox_loss_param.overlap_threshold();
	const bool use_prior_for_matching = multirbox_loss_param.use_prior_for_matching();
	const bool ignore_cross_boundary_rbox = multirbox_loss_param.ignore_cross_boundary_rbox();
	const bool regress_size = multirbox_loss_param.regress_size();
	float prior_width = -1, prior_height = -1;
	if (!regress_size)
	{
		prior_width = multirbox_loss_param.prior_width();
		prior_height = multirbox_loss_param.prior_height();
	}
	CHECK_EQ(use_prior_for_matching,true) <<"use_prior_for_matching must be true in recent version.";
	CHECK_EQ(share_location,true) <<"share_location must be true in recent version.";
	CHECK_EQ(ignore_cross_boundary_rbox,false) <<"ignore_cross_boundary_rbox must be false in recent version.";
	// Find the matches.
	int num = all_loc_preds.size();
	for (int i = 0; i < num; ++i)
	{
		const int label = -1;
		map<int, vector<int> > match_indices;
		map<int, vector<float> > match_overlaps;
		// Check if there is ground truth for current image.
		if (all_gt_rboxes.find(i) == all_gt_rboxes.end())
		{
			// There is no gt for current image. All predictions are negative.
			int num_pred = prior_rboxes.size();
			match_indices[label].resize(num_pred, -1);
			match_overlaps[label].resize(num_pred, 0.);
			all_match_indices->push_back(match_indices);
			all_match_overlaps->push_back(match_overlaps);
			continue;
		}
		// Find match between predictions and ground truth.
		const vector<NormalizedRBox>& gt_rboxes = all_gt_rboxes.find(i)->second;
		// Use prior rboxes to match against all ground truth.
		vector<int> temp_match_indices;
		vector<float> temp_match_overlaps;
		if (regress_size)
			MatchRBox(gt_rboxes, prior_rboxes, label, match_type, overlap_threshold,
				ignore_cross_boundary_rbox, &temp_match_indices,
				&temp_match_overlaps);
		else
			MatchRBox(gt_rboxes, prior_rboxes, label, match_type, overlap_threshold,
				ignore_cross_boundary_rbox, &temp_match_indices,
				&temp_match_overlaps, prior_width, prior_height);
		match_indices[label] = temp_match_indices;
		match_overlaps[label] = temp_match_overlaps;
		all_match_indices->push_back(match_indices);
		all_match_overlaps->push_back(match_overlaps);
	}
}

int CountNumMatchesR(const vector<map<int, vector<int> > >& all_match_indices,
	const int num) 
{
	int num_matches = 0;
	for (int i = 0; i < num; ++i)
	{
		const map<int, vector<int> >& match_indices = all_match_indices[i];
		for (map<int, vector<int> >::const_iterator it = match_indices.begin();
			it != match_indices.end(); ++it)
		{
			const vector<int>& match_index = it->second;
			for (int m = 0; m < match_index.size(); ++m)
			{
				if (match_index[m] > -1)
				{
					++num_matches;
				}
			}
		}
	}
	return num_matches;
}

template <typename Dtype>
void ComputeConfLossR(const Dtype* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	const int background_label_id, const ConfLossType loss_type,
	const vector<map<int, vector<int> > >& all_match_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	vector<vector<float> >* all_conf_loss) {
		CHECK_LT(background_label_id, num_classes);
		// CHECK_EQ(num, all_match_indices.size());
		all_conf_loss->clear();
		for (int i = 0; i < num; ++i) {
			vector<float> conf_loss;
			const map<int, vector<int> >& match_indices = all_match_indices[i];
			for (int p = 0; p < num_preds_per_class; ++p) {
				int start_idx = p * num_classes;
				// Get the label index.
				int label = background_label_id;
				for (map<int, vector<int> >::const_iterator it =
					match_indices.begin(); it != match_indices.end(); ++it) {
						const vector<int>& match_index = it->second;
						//LOG(INFO)<<match_index.size()<<" "<<num_preds_per_class;
						CHECK_EQ(match_index.size(), num_preds_per_class);
						if (match_index[p] > -1) {
							CHECK(all_gt_rboxes.find(i) != all_gt_rboxes.end());
							const vector<NormalizedRBox>& gt_rboxes =
								all_gt_rboxes.find(i)->second;
							CHECK_LT(match_index[p], gt_rboxes.size());
							label = gt_rboxes[match_index[p]].label();
							CHECK_GE(label, 0);
							CHECK_NE(label, background_label_id);
							//LOG(INFO)<<label<<" "<<num_classes;
							CHECK_LT(label, num_classes);
							// A prior can only be matched to one gt rbox.
							break;
						}
				}
				Dtype loss = 0;
				if (loss_type == MultiRBoxLossParameter_ConfLossType_SOFTMAX) {
					CHECK_GE(label, 0);
					CHECK_LT(label, num_classes);
					// Compute softmax probability.
					// We need to subtract the max to avoid numerical issues.
					Dtype maxval = conf_data[start_idx];
					for (int c = 1; c < num_classes; ++c) {
						maxval = std::max<Dtype>(conf_data[start_idx + c], maxval);
					}
					Dtype sum = 0.;
					for (int c = 0; c < num_classes; ++c) {
						sum += std::exp(conf_data[start_idx + c] - maxval);
					}
					Dtype prob = std::exp(conf_data[start_idx + label] - maxval) / sum;
					loss = -log(std::max(prob, Dtype(FLT_MIN)));
				} else if (loss_type == MultiRBoxLossParameter_ConfLossType_LOGISTIC) {
					int target = 0;
					for (int c = 0; c < num_classes; ++c) {
						if (c == label) {
							target = 1;
						} else {
							target = 0;
						}
						Dtype input = conf_data[start_idx + c];
						loss -= input * (target - (input >= 0)) -
							log(1 + exp(input - 2 * input * (input >= 0)));
					}
				} else {
					LOG(FATAL) << "Unknown conf loss type.";
				}
				conf_loss.push_back(loss);
			}
			conf_data += num_preds_per_class * num_classes;
			all_conf_loss->push_back(conf_loss);
		}
}

// Explicit initialization.
template void ComputeConfLossR(const float* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	const int background_label_id, const ConfLossType loss_type,
	const vector<map<int, vector<int> > >& all_match_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	vector<vector<float> >* all_conf_loss);
template void ComputeConfLossR(const double* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	const int background_label_id, const ConfLossType loss_type,
	const vector<map<int, vector<int> > >& all_match_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	vector<vector<float> >* all_conf_loss);


inline bool IsEligibleMiningR(const MiningType mining_type, const int match_idx,
	const float match_overlap, const float neg_overlap) {
		if (mining_type == MultiRBoxLossParameter_MiningType_MAX_NEGATIVE) {
			return match_idx == -1 && match_overlap < neg_overlap;
		} else if (mining_type == MultiRBoxLossParameter_MiningType_HARD_EXAMPLE) {
			return true;
		} else {
			return false;
		}
}


template <typename T>
bool SortScorePairDescend(const pair<float, T>& pair1,
                          const pair<float, T>& pair2) {
  return pair1.first > pair2.first;
}

// Explicit initialization.
template bool SortScorePairDescend(const pair<float, int>& pair1,
                                   const pair<float, int>& pair2);
template bool SortScorePairDescend(const pair<float, pair<int, int> >& pair1,
                                   const pair<float, pair<int, int> >& pair2);

template <typename Dtype>
void MineHardExamplesR(const Blob<Dtype>& conf_blob,
	const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const vector<map<int, vector<float> > >& all_match_overlaps,
	const MultiRBoxLossParameter& multirbox_loss_param,
	int* num_matches, int* num_negs,
	vector<map<int, vector<int> > >* all_match_indices,
	vector<vector<int> >* all_neg_indices)
{
	int num = all_loc_preds.size();
	// CHECK_EQ(num, all_match_overlaps.size());
	// CHECK_EQ(num, all_match_indices->size());
	// all_neg_indices->clear();
	*num_matches = CountNumMatchesR(*all_match_indices, num);
	*num_negs = 0;
	int num_priors = prior_rboxes.size();
	CHECK_EQ(num_priors, prior_variances.size());
	// Get parameters.
	CHECK(multirbox_loss_param.has_num_classes()) << "Must provide num_classes.";
	const int num_classes = multirbox_loss_param.num_classes();
	CHECK_GE(num_classes, 1) << "num_classes should not be less than 1.";
	const int background_label_id = multirbox_loss_param.background_label_id();
	//const bool use_prior_for_nms = multirbox_loss_param.use_prior_for_nms();
	const ConfLossType conf_loss_type = multirbox_loss_param.conf_loss_type();
	const MiningType mining_type = multirbox_loss_param.mining_type();
	if (mining_type == MultiRBoxLossParameter_MiningType_NONE) {
		return;
	}
	//const LocLossType loc_loss_type = multirbox_loss_param.loc_loss_type();
	const float neg_pos_ratio = multirbox_loss_param.neg_pos_ratio();
	const float neg_overlap = multirbox_loss_param.neg_overlap();
	//const CodeType code_type = multirbox_loss_param.code_type();
	//const bool encode_variance_in_target =
		multirbox_loss_param.encode_variance_in_target();
	const bool has_nms_param = multirbox_loss_param.has_nms_param();
	float nms_threshold = 0;
	const int sample_size = multirbox_loss_param.sample_size();
	// Compute confidence losses based on matching results.
	vector<vector<float> > all_conf_loss;
#ifdef CPU_ONLY
	ComputeConfLossR(conf_blob.cpu_data(), num, num_priors, num_classes,
		background_label_id, conf_loss_type, *all_match_indices, all_gt_rboxes,
		&all_conf_loss);
#else
	ComputeConfLossR(conf_blob.cpu_data(), num, num_priors, num_classes,
		background_label_id, conf_loss_type, *all_match_indices, all_gt_rboxes,
		&all_conf_loss);	
	//ComputeConfLossRGPU(conf_blob, num, num_priors, num_classes,
	//	background_label_id, conf_loss_type, *all_match_indices, all_gt_rboxes,
	//	&all_conf_loss);
#endif
	vector<vector<float> > all_loc_loss;
	if (mining_type == MultiRBoxLossParameter_MiningType_HARD_EXAMPLE) {
	} 
	else {
		// No localization loss.
		for (int i = 0; i < num; ++i) {
			vector<float> loc_loss(num_priors, 0.f);
			all_loc_loss.push_back(loc_loss);
		}
	}
	int accum_pos_num = 0;
	int accum_num = 0;
	for (int i = 0; i < num; ++i) {
		map<int, vector<int> >& match_indices = (*all_match_indices)[i];
		const map<int, vector<float> >& match_overlaps = all_match_overlaps[i];
		// loc + conf loss.
		const vector<float>& conf_loss = all_conf_loss[i];
		const vector<float>& loc_loss = all_loc_loss[i];
		vector<float> loss;
		std::transform(conf_loss.begin(), conf_loss.end(), loc_loss.begin(),
			std::back_inserter(loss), std::plus<float>());
		// Pick negatives or hard examples based on loss.
		set<int> sel_indices;
		vector<int> neg_indices;
		for (map<int, vector<int> >::iterator it = match_indices.begin();
			it != match_indices.end(); ++it) {
				const int label = it->first;
				int num_sel = 0;
				// Get potential indices and loss pairs.
				vector<pair<float, int> > loss_indices;
				for (int m = 0; m < match_indices[label].size(); ++m) {
					if (IsEligibleMiningR(mining_type, match_indices[label][m],
						match_overlaps.find(label)->second[m], neg_overlap)) {
							loss_indices.push_back(std::make_pair(loss[m], m));
							++num_sel;
					}
				}
				if (mining_type == MultiRBoxLossParameter_MiningType_MAX_NEGATIVE) {
					int num_pos = 0;
					for (int m = 0; m < match_indices[label].size(); ++m) {
						if (match_indices[label][m] > -1) {
							++num_pos;
						}
					}
					if(num_pos > 0) 
					{
						accum_pos_num += num_pos;
						accum_num ++;
						num_sel = std::min(static_cast<int>(num_pos * neg_pos_ratio), num_sel);  ////////////////
					}
					else
					{
						float ave_pos_num = 1.0;
						if (accum_num > 0) ave_pos_num = (float)accum_pos_num / accum_num; 
						num_sel = std::min(static_cast<int>((int)ceil(ave_pos_num)), num_sel);
						//num_sel = 1; 
					}
						
				} else if (mining_type == MultiRBoxLossParameter_MiningType_HARD_EXAMPLE) {
					CHECK_GT(sample_size, 0);
					num_sel = std::min(sample_size, num_sel);
				}
				// Select samples.
				if (has_nms_param && nms_threshold > 0) {
				} else {
					// Pick top example indices based on loss.
					std::sort(loss_indices.begin(), loss_indices.end(),
						SortScorePairDescend<int>);
					for (int n = 0; n < num_sel; ++n) {
						sel_indices.insert(loss_indices[n].second);
					}
				}
				// Update the match_indices and select neg_indices.
				for (int m = 0; m < match_indices[label].size(); ++m) {
					if (match_indices[label][m] > -1) {
						if (mining_type == MultiRBoxLossParameter_MiningType_HARD_EXAMPLE &&
							sel_indices.find(m) == sel_indices.end()) {
								match_indices[label][m] = -1;
								*num_matches -= 1;
						}
					} else if (match_indices[label][m] == -1) {
						if (sel_indices.find(m) != sel_indices.end()) {
							neg_indices.push_back(m);
							*num_negs += 1;
						}
					}
				}
		}
		all_neg_indices->push_back(neg_indices);
	}
}

// Explicite initialization.
template void MineHardExamplesR(const Blob<float>& conf_blob,
	const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const vector<map<int, vector<float> > >& all_match_overlaps,
	const MultiRBoxLossParameter& multirbox_loss_param,
	int* num_matches, int* num_negs,
	vector<map<int, vector<int> > >* all_match_indices,
	vector<vector<int> >* all_neg_indices);
template void MineHardExamplesR(const Blob<double>& conf_blob,
	const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const vector<map<int, vector<float> > >& all_match_overlaps,
	const MultiRBoxLossParameter& multirbox_loss_param,
	int* num_matches, int* num_negs,
	vector<map<int, vector<int> > >* all_match_indices,
	vector<vector<int> >* all_neg_indices);

void EncodeRBox(
	const NormalizedRBox& prior_rbox, const vector<float>& prior_variance,
	const CodeType code_type, const bool encode_variance_in_target,
	const NormalizedRBox& rbox, NormalizedRBox* encode_rbox,
	const bool regress_size, const bool regress_angle)
{
	if (code_type == PriorRBoxParameter_CodeType_CENTER_SIZE)
	{
		float prior_center_x = prior_rbox.xcenter();
		float prior_center_y = prior_rbox.ycenter();
		float rbox_center_x = rbox.xcenter();
		float rbox_center_y = rbox.ycenter();
		float prior_width = prior_rbox.width();
		float prior_height = prior_rbox.height();
		float rbox_width = prior_width;
		float rbox_height = prior_height;
		if (regress_size)
		{
			rbox_width = rbox.width();
			rbox_height = rbox.height();
		}
		float rbox_angle = 0, prior_angle = 0;
		if (regress_angle)
		{
			rbox_angle = rbox.angle();
			prior_angle = prior_rbox.angle();
		}

		if (encode_variance_in_target)
		{
			encode_rbox->set_xcenter((rbox_center_x - prior_center_x) / prior_width);
			encode_rbox->set_ycenter((rbox_center_y - prior_center_y) / prior_height);
			encode_rbox->set_width(log(rbox_width / prior_width));
			encode_rbox->set_height(log(rbox_height / prior_height));
			encode_rbox->set_angle(sin((rbox_angle - prior_angle) * 3.141593 / 180));
		} 
		else
		{
			// Encode variance in rbox.
			encode_rbox->set_xcenter((rbox_center_x - prior_center_x) / prior_width / prior_variance[0]);
			encode_rbox->set_ycenter((rbox_center_y - prior_center_y) / prior_height / prior_variance[1]);
			int count = 2;
			if (regress_size)
			{
				if (prior_variance.size() < 4) LOG(FATAL)<<"prior_variance mismatch!";
				encode_rbox->set_width(log(rbox_width / prior_width) / prior_variance[count]);
				count ++;
				encode_rbox->set_height(log(rbox_height / prior_height) / prior_variance[count]);
				count++;
			}
			if (regress_angle)
			{
				if (prior_variance.size() < count + 1) LOG(FATAL)<<"prior_variance mismatch!";
				if (cos((rbox_angle - prior_angle) * 3.141593 / 180) > 0)
					encode_rbox->set_angle(sin((rbox_angle - prior_angle) * 3.141593 / 180) / prior_variance[count]);
				else
					encode_rbox->set_angle(-sin((rbox_angle - prior_angle) * 3.141593 / 180) / prior_variance[count]);
			}
		}
	} 
	else
	{
		LOG(FATAL) << "Unknown LocLossType.";
	}
}


template <typename Dtype>
void EncodeLocPredictionR(const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const MultiRBoxLossParameter& multirbox_loss_param,
	Dtype* loc_pred_data, Dtype* loc_gt_data)
{
	int num = all_loc_preds.size();
	// CHECK_EQ(num, all_match_indices.size());
	// Get parameters.
	const CodeType code_type = multirbox_loss_param.code_type();
	const bool encode_variance_in_target =
		multirbox_loss_param.encode_variance_in_target();
	const bool bp_inside = multirbox_loss_param.bp_inside();
	const bool regress_size = multirbox_loss_param.regress_size();
	const bool regress_angle = multirbox_loss_param.regress_angle();
	int loc_size = 2;
	if (regress_size) loc_size += 2;
	if (regress_angle) loc_size ++;
	//const bool use_prior_for_matching =
	//	multirbox_loss_param.use_prior_for_matching();
	int count = 0;
	for (int i = 0; i < num; ++i) {
		for (map<int, vector<int> >::const_iterator
			it = all_match_indices[i].begin();
			it != all_match_indices[i].end(); ++it)
		{
			const int label = it->first;
			const vector<int>& match_index = it->second;
			CHECK(all_loc_preds[i].find(label) != all_loc_preds[i].end());
			const vector<NormalizedRBox>& loc_pred =
				all_loc_preds[i].find(label)->second;
			for (int j = 0; j < match_index.size(); ++j) {
				if (match_index[j] <= -1) {
					continue;
				}
				// Store encoded ground truth.
				const int gt_idx = match_index[j];
				CHECK(all_gt_rboxes.find(i) != all_gt_rboxes.end());
				CHECK_LT(gt_idx, all_gt_rboxes.find(i)->second.size());
				const NormalizedRBox& gt_rbox = all_gt_rboxes.find(i)->second[gt_idx];
				NormalizedRBox gt_encode;
				CHECK_LT(j, prior_rboxes.size());
				EncodeRBox(prior_rboxes[j], prior_variances[j], code_type,
					encode_variance_in_target, gt_rbox, &gt_encode, regress_size, regress_angle);
				loc_gt_data[count * loc_size] = gt_encode.xcenter();
				loc_gt_data[count * loc_size + 1] = gt_encode.ycenter();
				int counter = 2;
				if (regress_size)
				{
					loc_gt_data[count * loc_size + counter] = gt_encode.width();
					counter ++;
					loc_gt_data[count * loc_size + counter] = gt_encode.height();
					counter ++;
				}
				loc_gt_data[count * loc_size + counter] = gt_encode.angle();
				// Store location prediction.
				CHECK_LT(j, loc_pred.size());
				if (bp_inside) {
					LOG(FATAL) << "Can not support bp_inside in this version.";
				} 
				else
				{
					loc_pred_data[count * loc_size] = loc_pred[j].xcenter();
					loc_pred_data[count * loc_size + 1] = loc_pred[j].ycenter();
					counter = 2;
					if (regress_size)
					{
						loc_pred_data[count * loc_size + counter] = loc_pred[j].width();
						counter ++;
						loc_pred_data[count * loc_size + counter] = loc_pred[j].height();
						counter ++;
					}
					if (regress_angle)
						loc_pred_data[count * loc_size + counter] = loc_pred[j].angle();
				}
				if (encode_variance_in_target) {
					LOG(FATAL) << "Can not support encode_variance_in_target in this version.";
				}
				++count;
			}
		}
	}
}

// Explicit initialization.
template void EncodeLocPredictionR(const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const MultiRBoxLossParameter& multirbox_loss_param,
	float* loc_pred_data, float* loc_gt_data);
template void EncodeLocPredictionR(const vector<LabelRBox>& all_loc_preds,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const MultiRBoxLossParameter& multirbox_loss_param,
	double* loc_pred_data, double* loc_gt_data);

template <typename Dtype>
void EncodeConfPredictionR(const Dtype* conf_data, const int num,
	const int num_priors, const MultiRBoxLossParameter& multirbox_loss_param,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<vector<int> >& all_neg_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	Dtype* conf_pred_data, Dtype* conf_gt_data)
{
	// CHECK_EQ(num, all_match_indices.size());
	// CHECK_EQ(num, all_neg_indices.size());
	// Retrieve parameters.
	CHECK(multirbox_loss_param.has_num_classes()) << "Must provide num_classes.";
	const int num_classes = multirbox_loss_param.num_classes();
	CHECK_GE(num_classes, 1) << "num_classes should not be less than 1.";
	const int background_label_id = multirbox_loss_param.background_label_id();
	const bool map_object_to_agnostic =
		multirbox_loss_param.map_object_to_agnostic();
	if (map_object_to_agnostic) {
		if (background_label_id >= 0) {
			CHECK_EQ(num_classes, 2);
		} else {
			CHECK_EQ(num_classes, 1);
		}
	}
	const MiningType mining_type = multirbox_loss_param.mining_type();
	bool do_neg_mining;
	if (multirbox_loss_param.has_do_neg_mining()) {
		LOG(WARNING) << "do_neg_mining is deprecated, use mining_type instead.";
		do_neg_mining = multirbox_loss_param.do_neg_mining();
		CHECK_EQ(do_neg_mining,
			mining_type != MultiRBoxLossParameter_MiningType_NONE);
	}
	do_neg_mining = mining_type != MultiRBoxLossParameter_MiningType_NONE;
	const ConfLossType conf_loss_type = multirbox_loss_param.conf_loss_type();
	int count = 0;
	for (int i = 0; i < num; ++i) {
		if (all_gt_rboxes.find(i) != all_gt_rboxes.end()) 
		{
			// Save matched (positive) rboxes scores and labels.
			const map<int, vector<int> >& match_indices = all_match_indices[i];
			for (map<int, vector<int> >::const_iterator it =
				match_indices.begin(); it != match_indices.end(); ++it)
			{
				const vector<int>& match_index = it->second;
				CHECK_EQ(match_index.size(), num_priors);
				for (int j = 0; j < num_priors; ++j) {
					if (match_index[j] <= -1) {
						continue;
					}
					const int gt_label = map_object_to_agnostic ?
						background_label_id + 1 :
					all_gt_rboxes.find(i)->second[match_index[j]].label();
					int idx = do_neg_mining ? count : j;
					switch (conf_loss_type) {
					case MultiRBoxLossParameter_ConfLossType_SOFTMAX:
						conf_gt_data[idx] = gt_label;
						break;
					case MultiRBoxLossParameter_ConfLossType_LOGISTIC:
						conf_gt_data[idx * num_classes + gt_label] = 1;
						break;
					default:
						LOG(FATAL) << "Unknown conf loss type.";
					}
					if (do_neg_mining) {
						// Copy scores for matched rboxes.
						caffe_copy<Dtype>(num_classes, conf_data + j * num_classes,
							conf_pred_data + count * num_classes);
						++count;
					}
				}
			}
			// Go to next image.
			//LOG(INFO)<<"POS: count="<<count;
		}
		
		
		if (do_neg_mining) 
		{
			// Save negative rboxes scores and labels.
			for (int n = 0; n < all_neg_indices[i].size(); ++n) {
				int j = all_neg_indices[i][n];
				CHECK_LT(j, num_priors);
				caffe_copy<Dtype>(num_classes, conf_data + j * num_classes,
					conf_pred_data + count * num_classes);
				//LOG(INFO)<<"num="<<i<<"  neg_conf="
				switch (conf_loss_type) {
				case MultiRBoxLossParameter_ConfLossType_SOFTMAX:
					conf_gt_data[count] = background_label_id;
					break;
				case MultiRBoxLossParameter_ConfLossType_LOGISTIC:
					if (background_label_id >= 0 &&
						background_label_id < num_classes) {
							conf_gt_data[count * num_classes + background_label_id] = 1;
					}
					break;
				default:
					LOG(FATAL) << "Unknown conf loss type.";
				}
				++count;
			}
			//LOG(INFO)<<"NEG: count="<<count;
		}
			
			
		if (do_neg_mining) {
			conf_data += num_priors * num_classes;
		} else {
			conf_gt_data += num_priors;
		}
	}
}

// Explicite initialization.
template void EncodeConfPredictionR(const float* conf_data, const int num,
	const int num_priors, const MultiRBoxLossParameter& multirbox_loss_param,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<vector<int> >& all_neg_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	float* conf_pred_data, float* conf_gt_data);
template void EncodeConfPredictionR(const double* conf_data, const int num,
	const int num_priors, const MultiRBoxLossParameter& multirbox_loss_param,
	const vector<map<int, vector<int> > >& all_match_indices,
	const vector<vector<int> >& all_neg_indices,
	const map<int, vector<NormalizedRBox> >& all_gt_rboxes,
	double* conf_pred_data, double* conf_gt_data);



template <typename Dtype>
void GetConfidenceScoresR(const Dtype* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	vector<map<int, vector<float> > >* conf_preds)
{
	conf_preds->clear();
	conf_preds->resize(num);
	for (int i = 0; i < num; ++i) {
		map<int, vector<float> >& label_scores = (*conf_preds)[i];
		for (int p = 0; p < num_preds_per_class; ++p) {
			int start_idx = p * num_classes;
			for (int c = 0; c < num_classes; ++c) {
				label_scores[c].push_back(conf_data[start_idx + c]);
			}
		}
		conf_data += num_preds_per_class * num_classes;
	}
}

// Explicit initialization.
template void GetConfidenceScoresR(const float* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	vector<map<int, vector<float> > >* conf_preds);
template void GetConfidenceScoresR(const double* conf_data, const int num,
	const int num_preds_per_class, const int num_classes,
	vector<map<int, vector<float> > >* conf_preds);

void DecodeRBox(
	const NormalizedRBox& prior_rbox, const vector<float>& prior_variance,
	const CodeType code_type, const bool variance_encoded_in_target,
	const bool clip_rbox, const NormalizedRBox& rbox,
	const bool regress_size, const bool regress_angle,
	NormalizedRBox* decode_rbox)
{
	if (code_type == PriorRBoxParameter_CodeType_CENTER_SIZE)
	{
		float prior_center_x = prior_rbox.xcenter();
		float prior_center_y = prior_rbox.ycenter();
		float prior_angle = prior_rbox.angle();
		float prior_width = prior_rbox.width();
		float prior_height = prior_rbox.height();
		float rbox_center_x = rbox.xcenter();
		float rbox_center_y = rbox.ycenter();
		float rbox_width = prior_width;
		float rbox_height = prior_height;
		float rbox_angle = 0;
		if (regress_size)
		{
			rbox_width = rbox.width();
			rbox_height = rbox.height();
		}
		if (regress_angle)
			rbox_angle = rbox.angle();
		float decode_rbox_center_x, decode_rbox_center_y;
		float decode_rbox_width, decode_rbox_height;
		float decode_rbox_angle;
		if (!regress_size)
		{
			decode_rbox_width = prior_width;
			decode_rbox_height = prior_height;
		}
		if (!regress_angle)
			decode_rbox_angle = 0;
		if (variance_encoded_in_target) 
		{
			// variance is encoded in target, we simply need to retore the offset
			// predictions.
			decode_rbox_center_x = rbox_center_x * prior_width + prior_center_x;
			decode_rbox_center_y = rbox_center_y * prior_height + prior_center_y;
			if (regress_size)
			{
				decode_rbox_width = exp(rbox_width) * prior_width;
				decode_rbox_height = exp(rbox_height) * prior_height;
			}
			if (regress_angle)
			{
				if (rbox_angle > 1)  rbox_angle = 1;
				if (rbox_angle < -1) rbox_angle = -1;
				decode_rbox_angle = asin(rbox_angle) * 180 / 3.141593 + prior_angle;
			}
		} 
		else
		{
			// variance is encoded in rbox, we need to scale the offset accordingly.
			decode_rbox_center_x =
				prior_variance[0] * rbox_center_x * prior_width + prior_center_x;
			decode_rbox_center_y =
				prior_variance[1] * rbox_center_y * prior_height + prior_center_y;
			int count = 2;
			if (regress_size)
			{
				decode_rbox_width = exp(rbox_width * prior_variance[count]) * prior_width;
				count ++;
				decode_rbox_height = exp(rbox_height * prior_variance[count]) * prior_height;
				count ++;
			}
			if (regress_angle)
			{
				rbox_angle *= prior_variance[count];
				if (rbox_angle > 1)  rbox_angle = 1;
				if (rbox_angle < -1) rbox_angle = -1;
				decode_rbox_angle =
					asin(rbox_angle) * 180 / 3.141593 + prior_angle;
			}
		}

		decode_rbox->set_xcenter(decode_rbox_center_x);
		decode_rbox->set_ycenter(decode_rbox_center_y);
		decode_rbox->set_angle(decode_rbox_angle);
		decode_rbox->set_width(decode_rbox_width);
		decode_rbox->set_height(decode_rbox_height);
		float rbox_size = decode_rbox_width * decode_rbox_height;
		decode_rbox->set_size(rbox_size);
	} 
	else
	{
		LOG(FATAL) << "Unknown LocLossType.";
	}
}

void DecodeRBoxes(
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const CodeType code_type, const bool variance_encoded_in_target,
	const bool clip_rbox, const vector<NormalizedRBox>& rboxes,
	const bool regress_size, const bool regress_angle,
	vector<NormalizedRBox>* decode_rboxes) {
		CHECK_EQ(prior_rboxes.size(), prior_variances.size());
		CHECK_EQ(prior_rboxes.size(), rboxes.size());
		int para_size = 2;
		if (regress_size) para_size += 2;
		if (regress_angle) para_size ++; 
		int num_rboxes = prior_rboxes.size();
		if (num_rboxes >= 1) {
			CHECK_EQ(prior_variances[0].size(), para_size);
		}
		decode_rboxes->clear();
		for (int i = 0; i < num_rboxes; ++i) {
			NormalizedRBox decode_rbox;
			DecodeRBox(prior_rboxes[i], prior_variances[i], code_type,
				variance_encoded_in_target, clip_rbox, rboxes[i], regress_size, regress_angle,
				&decode_rbox);
			decode_rboxes->push_back(decode_rbox);
		}
}

void DecodeRBoxesAll(const vector<LabelRBox>& all_loc_preds,
	const vector<NormalizedRBox>& prior_rboxes,
	const vector<vector<float> >& prior_variances,
	const int num, const bool share_location,
	const int num_loc_classes, const int background_label_id,
	const CodeType code_type, const bool variance_encoded_in_target,
	const bool clip, const bool regress_size, const bool regress_angle,
	vector<LabelRBox>* all_decode_rboxes)
{
	CHECK_EQ(all_loc_preds.size(), num);
	all_decode_rboxes->clear();
	all_decode_rboxes->resize(num);
	omp_set_num_threads(16);
	#pragma omp parallel for
	for (int i = 0; i < num; ++i) {
		// Decode predictions into rboxes.
		LabelRBox& decode_rboxes = (*all_decode_rboxes)[i];
		for (int c = 0; c < num_loc_classes; ++c) {
			int label = share_location ? -1 : c;
			if (label == background_label_id) {
				// Ignore background class.
				continue;
			}
			if (all_loc_preds[i].find(label) == all_loc_preds[i].end()) {
				// Something bad happened if there are no predictions for current label.
				LOG(FATAL) << "Could not find location predictions for label " << label;
			}
			const vector<NormalizedRBox>& label_loc_preds =
				all_loc_preds[i].find(label)->second;
			DecodeRBoxes(prior_rboxes, prior_variances,
				code_type, variance_encoded_in_target, clip,
				label_loc_preds, regress_size, regress_angle,
				&(decode_rboxes[label]));
		}
	}
}

template <typename Dtype>
void GetMaxScoreIndex(const Dtype* scores, const int num, const float threshold,
      const int top_k, vector<pair<Dtype, int> >* score_index_vec) 
{
  // Generate index score pairs.
  for (int i = 0; i < num; ++i) {
    if (scores[i] > threshold) {
      score_index_vec->push_back(std::make_pair(scores[i], i));
    }
  }

  // Sort the score pair according to the scores in descending order
  std::sort(score_index_vec->begin(), score_index_vec->end(),
            SortScorePairDescend<int>);

  // Keep top_k scores if needed.
  if (top_k > -1 && top_k < score_index_vec->size()) {
    score_index_vec->resize(top_k);
  }
}


void GetMaxScoreIndexR(const vector<float>& scores, const float threshold,
      const int top_k, vector<pair<float, int> >* score_index_vec) {
  // Generate index score pairs.
  for (int i = 0; i < scores.size(); ++i) {
    if (scores[i] > threshold) {
      score_index_vec->push_back(std::make_pair(scores[i], i));
    }
  }

  // Sort the score pair according to the scores in descending order
  std::stable_sort(score_index_vec->begin(), score_index_vec->end(),
                   SortScorePairDescend<int>);

  // Keep top_k scores if needed.
  if (top_k > -1 && top_k < score_index_vec->size()) {
    score_index_vec->resize(top_k);
  }
}


void ApplyNMSFastR(const vector<NormalizedRBox>& rboxes,
      const vector<float>& scores, const float score_threshold,
      const float nms_threshold, const float eta, const int top_k,
      vector<int>* indices)
{
	// Get top_k scores (with corresponding indices).
	vector<pair<float, int> > score_index_vec;
	GetMaxScoreIndexR(scores, score_threshold, top_k, &score_index_vec);

	// Do nms.
	float adaptive_threshold = nms_threshold;
	indices->clear();
	while (score_index_vec.size() != 0) 
	{
		const int idx = score_index_vec.front().second;
		bool keep = true;
		for (int k = 0; k < indices->size(); ++k) 
		{
			if (keep)
			{
				const int kept_idx = (*indices)[k];
				float overlap = JaccardOverlapRR(rboxes[idx], rboxes[kept_idx]);
				keep = overlap <= adaptive_threshold;
			} 
			else
			{
				break;
			}
		}
		if (keep)
		{
			indices->push_back(idx);
		}
		score_index_vec.erase(score_index_vec.begin());
		if (keep && eta < 1 && adaptive_threshold > 0.5) 
		{
			adaptive_threshold *= eta;
		}
	}
}


template <typename Dtype>
void GetRDetectionResults(const Dtype* det_data, const int num_det,
	const int background_label_id,
	map<int, map<int, vector<NormalizedRBox> > >* all_detections) 
{
	all_detections->clear();
	for (int i = 0; i < num_det; ++i) {
		int start_idx = i * 8;
		int item_id = det_data[start_idx];
		if (item_id == -1) {
			continue;
		}
		int label = det_data[start_idx + 1];
		CHECK_NE(background_label_id, label)
			<< "Found background label in the detection results.";
		NormalizedRBox rbox;
		rbox.set_score(det_data[start_idx + 2]);
		rbox.set_xcenter(det_data[start_idx + 3]);
		rbox.set_ycenter(det_data[start_idx + 4]);
		rbox.set_angle(det_data[start_idx + 5]);
		rbox.set_width(det_data[start_idx + 6]);
		rbox.set_height(det_data[start_idx + 7]);
		float rbox_size = det_data[start_idx + 6] * det_data[start_idx + 7];
		rbox.set_size(rbox_size);
		(*all_detections)[item_id][label].push_back(rbox);
	}
}

// Explicit initialization.
template void GetRDetectionResults(const float* det_data, const int num_det,
	const int background_label_id,
	map<int, map<int, vector<NormalizedRBox> > >* all_detections);
template void GetRDetectionResults(const double* det_data, const int num_det,
	const int background_label_id,
	map<int, map<int, vector<NormalizedRBox> > >* all_detections);

}// namespace caffe
