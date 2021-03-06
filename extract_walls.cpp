//
// Created by czh on 10/17/18.
//
#include <iostream>
#include <vector>
#include <cmath>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/obj_io.h> 
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <pcl/features/normal_3d.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/segmentation/sac_segmentation.h> 
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/project_inliers.h> 
#include <pcl/filters/passthrough.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/voxel_grid.h> 
#include <pcl/common/pca.h>
#include <pcl/common/common.h>
#include <pcl/common/geometry.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <yaml-cpp/yaml.h>
#include "extract_walls.h"
#include "Model.h"
#include "Plane.h"
#include "SimpleView.h"
#include "Reconstruction.h"

using namespace std;
typedef pcl::PointXYZRGB PointRGB;
typedef pcl::PointXYZRGBNormal PointT;
typedef pcl::PointCloud<PointT> PointCloudT;



/**
 * Gets a distance between two planes.
 *
 * @param [in,out] a A Plane to process.
 * @param [in,out] b A Plane to process.
 *
 * @return The distance.
 * @remark defind as line to line distance
 */
float getDistance(Plane& a, Plane& b) {
	float k = (a.leftUp().y  - a.rightUp().y) / (a.leftUp().x - a.rightUp().x);
	float b0 = a.leftUp().y  - k * a.leftUp().x;
	float b1 = b.leftUp().y  - k * b.leftUp().x;
	float b2 = b.rightUp().y - k * b.rightUp().x;

	float dist1 = abs(b0 - b1) / sqrt(k*k + 1);
	float dist2 = abs(b0 - b2) / sqrt(k*k + 1);
	// since this defination might not be good, we calculate two time. dist1 and dist2 should be so different
	if (abs(dist1 - dist2) > 1) PCL_WARN("dist1 and dist2 should be so different");
	return (dist1 + dist2) / 2;
}

/**
 * Query if 'a' is overlap with 'b'.
 *
 * @param [in,out] a A Plane to process.
 * @param [in,out] b A Plane to process.
 *
 * @return True if overlap, false if not.
 */
bool isOverlap(Plane& a, Plane& b) {
	float k0 = (a.leftUp().y - a.rightUp().y) / (a.leftUp().x - a.rightUp().x);
	float b0 = a.leftUp().y - k0 * a.leftUp().x;

	float k1 = -1 / k0;
	float b1 = b.leftUp().y  - k1 * b.leftUp().x;
	float b2 = b.rightUp().y - k1 * b.rightUp().x;

	Eigen::Matrix2f A;
	A << k0, -1, k1, -1;
	Eigen::Vector2f B0,B1;
	B0 << -b0, -b1;
	B1 << -b0, -b2;
	Eigen::Vector2f p1,p2;
	p1 = A.colPivHouseholderQr().solve(B0);
	p2 = A.colPivHouseholderQr().solve(B1);
	float largeX = a.leftUp().x > a.rightUp().x ? a.leftUp().x : a.rightUp().x;
	float smallX = a.leftUp().x < a.rightUp().x ? a.leftUp().x : a.rightUp().x;
	float largeY = a.leftUp().y > a.rightUp().y ? a.leftUp().y : a.rightUp().y;
	float smallY = a.leftUp().y < a.rightUp().y ? a.leftUp().y : a.rightUp().y;
	return (p1[0] >= smallX && p1[0] <= largeX) || (p1[1] >= smallY && p1[1] <= largeY);
	
}

/**
 * Searches for the near planes.
 *
 * @param [in,out] source   Source for the.
 * @param [in,out] planes   The planes.
 * @param 		   distance The distance.
 *
 * @return Null if it fails, else the found near plane.
 */
vector<Plane*> findNearPlanes(int source, vector<Plane>& planes, float distance) {
	vector<Plane*> res;
	for (int i = 0; i < planes.size(); ++i ) {
		if (source == i) continue;
		PointT s_l, s_r, t_l,t_r;
		s_l = planes[source].leftUp();
		s_r = planes[source].rightUp();
		s_l.z = 0; s_r.z = 0;
		t_l = planes[i].leftUp();
		t_r = planes[i].rightUp();
		t_l.z = 0; t_r.z = 0;
		if (pcl::geometry::distance(s_l, t_l) <= distance || pcl::geometry::distance(s_l, t_r) <= distance ||
			pcl::geometry::distance(s_r, t_l) <= distance || pcl::geometry::distance(s_r, t_r) <= distance) {
			res.push_back(&planes[i]);
		}
	}
	return res;
}

/**
 * Links two edges of two planes.
 *
 * @param [in,out] s_p The a plane.
 * @param 		   s_e The linked edge of a plane.
 * @param [in,out] t_p The b plane.
 * @param 		   t_e The linked edge of b plane.
 */
 
 /*
void linkEdge(Plane& s_p, edgeType s_e, Plane& t_p, edgeType t_e) {
	if (s_e == edgeType::EdgeLeft) {
		s_p.leftEdge.connectedPlane = &t_p;
		s_p.leftEdge.connectedEdgeType = t_e;
	}else if (s_e == edgeType::EdgeRight) {
		s_p.rightEdge.connectedPlane = &t_p;
		s_p.rightEdge.connectedEdgeType = t_e;
	}
	if (t_e == edgeType::EdgeLeft) {
		t_p.leftEdge.connectedPlane = &s_p;
		t_p.leftEdge.connectedEdgeType = s_e;
	}
	else if (t_e == edgeType::EdgeRight) {
		t_p.rightEdge.connectedPlane = &s_p;
		t_p.rightEdge.connectedEdgeType = s_e;
	}
}

void connectTwoEdge(Plane& plane_a, edgeType edge_a, Plane& plane_b, edgeType edge_b, Plane& output) {
	Plane tmp;
	if (edge_a == EdgeLeft && edge_b == EdgeLeft) {
		tmp.extendPlane(plane_a.leftDown(), plane_a.leftUp(),
			plane_b.leftDown(), plane_b.leftUp(), paras.pointPitch, Color_Red);
		plane_a.leftEdge.isConnected = true;
		plane_b.leftEdge.isConnected = true;
	}
	else if (edge_a == EdgeLeft && edge_b == EdgeRight) {
		tmp.extendPlane(plane_a.leftDown(), plane_a.leftUp(),
			plane_b.rightDown(), plane_b.rightUp(), paras.pointPitch, Color_Red);
		plane_a.leftEdge.isConnected = true;
		plane_b.rightEdge.isConnected = true;
	}
	else if (edge_a == EdgeRight && edge_b == EdgeLeft) {
		tmp.extendPlane(plane_a.rightDown(), plane_a.rightUp(),
			plane_b.leftDown(), plane_b.leftUp(), paras.pointPitch, Color_Red);
		plane_a.rightEdge.isConnected = true;
		plane_b.leftEdge.isConnected = true;
	}
	else if (edge_a == EdgeRight && edge_b == EdgeRight) {
		tmp.extendPlane(plane_a.rightDown(), plane_a.rightUp(),
			plane_b.rightDown(), plane_b.rightUp(), paras.pointPitch, Color_Red);
		plane_a.rightEdge.isConnected = true;
		plane_b.rightEdge.isConnected = true;
	}

}
*/
struct reconstructParas
{
	// Downsampling
	int KSearch = 0;
	float leafSize = 0; // unit is meter -> 5cm
	// Plane height threshold
	float minPlaneHeight = 0;
	// Clustering
	int MinSizeOfCluster = 0;
	int NumberOfNeighbours = 0;
	int SmoothnessThreshold = 0; // angle 360 degree
	int CurvatureThreshold = 0;
	// RANSAC
	double RANSAC_DistThreshold = 0; //0.25;
	float RANSAC_MinInliers = 0; // 500 todo: should be changed to percents
	float RANSAC_PlaneVectorThreshold = 0;

	// Fill the plane
	int pointPitch = 0; // number of point in 1 meter

	// Combine planes
	float minimumEdgeDist = 0; //we control the distance between two edges and the height difference between two edges
	float minPlanesDist = 0; // when clustering RANSAC planes, the min distance between two planes
	float minAngle_normalDiff = 0;// when extend smaller plane to bigger plane, we will calculate the angle between normals of planes
}paras;

// color
PlaneColor commonPlaneColor = Color_White;
PlaneColor outerPlaneColor = Color_Yellow;
PlaneColor innerPlaneColor = Color_Blue;
PlaneColor upDownPlaneColor = Color_Green;
void importConfig(const YAML::Node& node, reconstructParas &para);

int main(int argc, char** argv) {
	PCL_WARN("This program is based on assumption that ceiling and ground on the X-Y  \n");
	pcl::console::setVerbosityLevel(pcl::console::L_ALWAYS); // for not show the warning
	PointCloudT::Ptr allCloudFilled(new PointCloudT);
	vector<Plane> filledPlanes;
	vector<Plane> horizontalPlanes;
	vector<Plane> upDownPlanes;
	vector<Plane> wallEdgePlanes;
	string configPath;
	#ifdef _WIN32
		
		int index;
		
		for (int i = 1; i < argc; ++i) {
			cout << i << ": " << argv[i] << "\n";
		}
		cout << "Please enter an file index: ";
		cin >> index;
		cout << "\n";
		while (index >= argc) { 
			cerr << "worng number, pls select again" << endl; 
			cout << "Please enter an file index: ";
			cin >> index;
			cout << "\n";
		}
		string fileName = argv[index];
	#elif defined __unix__
		string fileName = "/home/czh/Desktop/pointCloud PartTime/test/Room_E_Cloud_binary.ply";
		if(argv[1] != "") fileName = argv[1];
        YAML::Node config = YAML::LoadFile(argv[2] == "" ? "./config.yaml" : argv[2]);
        importConfig(config,paras);
    #endif
    assert(argv[2] != "");
		cout << "\n***** start proceeing *****" << "\n";
	Reconstruction re(fileName);
	re.downSampling(paras.leafSize);
	re.applyRegionGrow(paras.NumberOfNeighbours, paras.SmoothnessThreshold,
		paras.CurvatureThreshold, paras.MinSizeOfCluster, paras.KSearch);
	re.applyRANSACtoClusters(paras.RANSAC_DistThreshold, paras.RANSAC_PlaneVectorThreshold, paras.RANSAC_MinInliers);
	PointCloudT::Ptr all(new PointCloudT);
	vector<Plane>& planes = re.ransacPlanes;
	for (auto &plane : planes) {
		if (plane.orientation == Horizontal) {
			horizontalPlanes.push_back(plane);
		}
		else if (plane.orientation == Vertical) {
			plane.filledPlane(paras.pointPitch);
			filledPlanes.push_back(plane);
		}
	}
	simpleView("Filled RANSAC planes", planes);

	// choose the two that have larger points as roof and ground
	for (size_t j = 0; j < horizontalPlanes.size() < 2 ? horizontalPlanes.size() : 2; j++) {
		size_t maxNum = 0;
		size_t maxCloudIndex = 0;
		for (size_t i = 0; i < horizontalPlanes.size(); ++i) {
			if (maxNum < horizontalPlanes[i].pointCloud->size()) {
				maxCloudIndex = i;
				maxNum = horizontalPlanes[i].pointCloud->size();
			}
		}
		upDownPlanes.push_back(horizontalPlanes[maxCloudIndex]);
		horizontalPlanes.erase(horizontalPlanes.begin() + maxCloudIndex);
	}
	cout << "num of horizontal planes: " << horizontalPlanes.size() << endl;
	cout << "num of upDownPlanes planes: " << upDownPlanes.size() << endl;

	// compute room height
	Eigen::Vector2f ZLimits(-upDownPlanes[0].abcd()[3], -upDownPlanes[1].abcd()[3]);
	if (upDownPlanes[0].abcd()[3] < upDownPlanes[1].abcd()[3]) {
		ZLimits[0] = -upDownPlanes[1].abcd()[3];
		ZLimits[1] = -upDownPlanes[0].abcd()[3];
	}
	cout << "\nHeight of room is " << ZLimits[1] - ZLimits[0] << endl;

	// remove planes whose height are not meet condition
	// remove planes whose has no near planes
	for (size_t i = 0; i < filledPlanes.size(); i++)
	{
		if ((filledPlanes[i].leftUp().z - filledPlanes[i].leftDown().z) / (ZLimits[1] - ZLimits[0]) <= paras.minPlaneHeight) {
			filledPlanes.erase(filledPlanes.begin() + i--);
		}
	}
	for (size_t i = 0; i < filledPlanes.size(); i++)
	{
		int size = findNearPlanes(i, filledPlanes, paras.minimumEdgeDist).size();
		if (size == 0) {
			filledPlanes.erase(filledPlanes.begin() + i--);
		}
	}

	
	int G_index = -1;
	for (Plane&plane_s : filledPlanes) {
		if (plane_s.group_index != -1) continue;
		G_index++;
		plane_s.group_index = G_index;
		stack<Plane*> tmp;
		tmp.push(&plane_s);
		while (!tmp.empty())
		{
			Plane* p_s = tmp.top();
			tmp.pop();
			for (Plane& p : filledPlanes) {
				Plane* p_t = &p;
				if (p_t->group_index != -1) continue;

				// angle of normal difference should lower than certain value
				double angle = acos(p_s->getNormal().dot(p_t->getNormal()) / (p_s->getNormal().norm()*p_t->getNormal().norm())) * 180 / M_PI;
				if (angle > paras.minAngle_normalDiff) continue;

				// distance between two planes should smaller enough
				if (getDistance(*p_s, *p_t) > paras.minPlanesDist) continue;

				// two planes should not overlap with each other
				if (!(isOverlap(*p_s, *p_t)) && !(isOverlap(*p_t, *p_s))) continue;
				p_t->group_index = G_index;
				tmp.push(p_t);
			}
		}
	}
	
	vector<int32_t> colors;
	for (size_t i = 0; i < G_index+1; i++)
	{
		int32_t r = rand() % 255;
		int32_t g = rand() % 255;
		int32_t b = rand() % 255;
		int32_t a = 255;
		a = a << 24;
		r = r << 16;
		g = g << 8;
		colors.push_back(a | r | g | b);
	}

	for (size_t i = 0; i < filledPlanes.size(); i++)
	{
		if(filledPlanes[i].group_index != -1) filledPlanes[i].setColor(colors[filledPlanes[i].group_index]);
	}
//<<<<<<< Updated upstream


	vector<Plane> planeGroup;
	vector<int> numOfGroups(G_index+1, 0);
	for (size_t i = 0; i <= G_index; i++)
	{
		PointCloudT::Ptr tmp(new PointCloudT);
		for (auto &plane : filledPlanes) {
			if (plane.group_index != i) continue;
			numOfGroups[i]+=1;
			for (auto &p : plane.pointCloud->points) tmp->push_back(p);
//=======
//	PointCloudT::Ptr roofClusterPts(new PointCloudT);
//	for (int l = 0; l < roofEdgeClusters.size(); ++l) {
//		int color = 255 <<24 | colors[l][0] << 16 | colors[l][1] << 8 | colors[l][2];
//        roofEdgeClustersCoffs[l]
//		for (auto &p:roofEdgeClusters[l]->points) {
//			p.rgba = color;
//			roofClusterPts->push_back(p);
//>>>>>>> Stashed changes
		}
		Plane plane(tmp);
		plane.group_index = i;
		planeGroup.push_back(plane);
	}

	simpleView("Filled RANSAC planes : Group Planes", planeGroup);
	
	cout << "\nHeight Filter: point lower than " << ZLimits[0] << " and higher than " << ZLimits[1] << endl;
	for (Plane&plane:planeGroup)
	{
		plane.runRANSAC(paras.RANSAC_DistThreshold, 0.8);
		plane.filledPlane(paras.pointPitch, ZLimits[1], ZLimits[0]);
	}
	simpleView("Filled RANSAC planes : Filled Group Planes", planeGroup);
	/*
	// find nearest edges
	vector<vector<int>> record(planeGroup.size(), vector<int>(2,-1));
	for (int i = 0; i < planeGroup.size(); ++i) {
		Plane* plane_s = &planeGroup[i];
		vector<float> tmpLeftEdgesDist(planeGroup.size()*2,INT32_MAX);
		vector<float> tmpRightEdgesDist(planeGroup.size()*2, INT32_MAX);
		for (int j = 0; j < planeGroup.size(); ++j) {
			if (i == j) continue;
				
			Plane* plane_t = &planeGroup[j];
			float ll = (pcl::geometry::distance(plane_s->leftDown(),  plane_t->leftDown()));
			float lr = (pcl::geometry::distance(plane_s->leftDown(),  plane_t->rightDown()));
			float rl = (pcl::geometry::distance(plane_s->rightDown(), plane_t->leftDown()));
			float rr = (pcl::geometry::distance(plane_s->rightDown(), plane_t->rightDown()));
			tmpLeftEdgesDist[2*j] = ll ;
			tmpLeftEdgesDist[2*j+1] = lr ;
			tmpRightEdgesDist[2*j] = rl;
			tmpRightEdgesDist[2*j+1] = rr;
		}
		auto minLeft = min_element(tmpLeftEdgesDist.begin(), tmpLeftEdgesDist.end());
		int minIndex_left = distance(tmpLeftEdgesDist.begin(), minLeft);
		record[i][0] = *minLeft > paras.minimumEdgeDist ? -1 : (minIndex_left);

		if (*minLeft <= paras.minimumEdgeDist) {
			edgeType type = minIndex_left % 2 == 0 ? EdgeLeft : EdgeRight;
			linkEdge(*plane_s, EdgeLeft, planeGroup[minIndex_left], type);
		}
		

		auto minRight = min_element(tmpRightEdgesDist.begin(), tmpRightEdgesDist.end());
		int minIndex_right = distance(tmpRightEdgesDist.begin(),minRight);
		record[i][1] = *minRight > paras.minimumEdgeDist ? -1 : (minIndex_right);

		if (*minRight <= paras.minimumEdgeDist) {
			edgeType type = minIndex_right % 2 == 0 ? EdgeLeft : EdgeRight;
			linkEdge(*plane_s, EdgeRight, planeGroup[minIndex_right], type);
		}
	}
	

	for (Plane &plane : planeGroup) {
		if (plane.leftEdge.connectedPlane && !plane.leftEdge.isConnected) {
			Plane tmp;
			connectTwoEdge(plane, EdgeLeft, *plane.leftEdge.connectedPlane, plane.leftEdge.connectedEdgeType, tmp);
			wallEdgePlanes.push_back(tmp);
		}
		
		if (plane.rightEdge.connectedPlane && !plane.rightEdge.isConnected){
			Plane tmp;
			connectTwoEdge(plane, EdgeRight, *plane.rightEdge.connectedPlane, plane.rightEdge.connectedEdgeType, tmp);
			wallEdgePlanes.push_back(tmp);
		}
	}

	
	for (int i = 0; i < planeGroup.size(); ++i) {
		if (record[i][0] == -1) continue;
		
		//for faster if two edges connect each other
		if (record[record[i][0] / 2][record[i][0] % 2] == 2*i) record[record[i][0] / 2][record[i][0] % 2] = -1;
		
		int leftTargetPlane = record[i][0] / 2;
		if (record[i][0] % 2 == 0) {
			Plane filled(planeGroup[i].leftDown(), planeGroup[i].leftUp(),
				planeGroup[leftTargetPlane].leftDown(), planeGroup[leftTargetPlane].leftUp(), paras.pointPitch, Color_Red);
			wallEdgePlanes.push_back(filled);
		}
		else if (record[i][0] % 2 == 1) {
			Plane filled(planeGroup[i].leftDown(), planeGroup[i].leftUp(),
				planeGroup[leftTargetPlane].rightDown(), planeGroup[leftTargetPlane].rightUp(), paras.pointPitch, Color_Red);
			wallEdgePlanes.push_back(filled);
		}


		if (record[i][1] == -1) continue;
		if (record[record[i][0] / 2][record[i][0] % 2] == 2 * i+1) record[record[i][0] / 2][record[i][0] % 2] = -1;
		int rightTargetPlane = record[i][1] / 2;
		if (record[i][1] % 2 == 0) {
			Plane filled(planeGroup[i].rightDown(), planeGroup[i].rightUp(),
				planeGroup[rightTargetPlane].leftDown(), planeGroup[rightTargetPlane].leftUp(), paras.pointPitch, Color_Red);
			wallEdgePlanes.push_back(filled);
		}
		else if (record[i][1] % 2 == 1) {
			Plane filled(planeGroup[i].rightDown(), planeGroup[i].rightUp(),
				planeGroup[rightTargetPlane].rightDown(), planeGroup[rightTargetPlane].rightUp(), paras.pointPitch, Color_Red);
			wallEdgePlanes.push_back(filled);
		}
		
	}
	*/
		
	for (auto &plane:planeGroup)
	{
		for (auto p : plane.pointCloud->points)
			allCloudFilled->push_back(p);
	}
	for (auto &plane : wallEdgePlanes)
	{
		for (auto p : plane.pointCloud->points)
			allCloudFilled->push_back(p);
	}
	simpleView("Connect wall planes", allCloudFilled);
	
	
	// mark: since we found the covered planes, we next extend these smaller planes to their covered planes.
	vector<Plane> extenedPlanes;
	int i = 0, j = 0;
	for (auto &plane_s : planeGroup) {
		if (numOfGroups[plane_s.group_index] == 1) continue;
		Plane tmp;
		for (auto &plane_t:filledPlanes)
		{
			if (plane_t.group_index != plane_s.group_index) continue;
			extendSmallPlaneToBigPlane(plane_t, plane_s, 4294951115, paras.pointPitch, tmp.pointCloud);
			
		}
		extenedPlanes.push_back(tmp);
	}

	allCloudFilled->resize(0);
	for (auto &plane : planeGroup)
	{
		for (auto p : plane.pointCloud->points)
			allCloudFilled->push_back(p);
	}
	for (auto &plane : wallEdgePlanes)
	{
		for (auto p : plane.pointCloud->points)
			allCloudFilled->push_back(p);
	}
	for (auto &plane : filledPlanes)
	{
		plane.setColor(PlaneColor::Color_Blue);
		for (auto p : plane.pointCloud->points)
			allCloudFilled->push_back(p);
	}
	simpleView("Extended Planes", allCloudFilled);
	PointCloudT::Ptr roof(new PointCloudT);
	// fill the ceiling and ground
	{
		PointT min, max;
		float step = 1 / (float)paras.pointPitch;
		pcl::getMinMax3D(*allCloudFilled, min, max);
		PointCloudT::Ptr topTemp(new PointCloudT);
		PointCloudT::Ptr downTemp(new PointCloudT);
		pcl::copyPointCloud(*allCloudFilled, *topTemp);
		pcl::copyPointCloud(*allCloudFilled, *downTemp);
		pcl::PassThrough<PointT> filterZ;
		filterZ.setInputCloud(topTemp);
		filterZ.setFilterFieldName("z");
		filterZ.setFilterLimits(max.z - 2 * step, max.z);
		filterZ.filter(*topTemp);
		filterZ.setInputCloud(downTemp);
		filterZ.setFilterFieldName("z");
		filterZ.setFilterLimits(min.z, min.z + 2 * step);
		filterZ.filter(*downTemp);
		PointT minTop, maxTop;
		pcl::getMinMax3D(*topTemp, minTop, maxTop);

		simpleView("top", topTemp);
		simpleView("down", downTemp);
		
		for (float i = minTop.x; i < maxTop.x; i += step) { // NOLINT
			// extract x within [i,i+step] -> tempX
			PointCloudT::Ptr topTempX(new PointCloudT);
			PointCloudT::Ptr downTempX(new PointCloudT);
			pcl::PassThrough<PointT> filterX;
			filterX.setInputCloud(topTemp);
			filterX.setFilterFieldName("x");
			filterX.setFilterLimits(i, i + 0.1);
			filterX.filter(*topTempX);
			filterX.setInputCloud(downTemp);
			filterX.filter(*downTempX);
			if (downTemp->size() < 2) continue;
			cout << "x " << i << "\n";
			// found the minY and maxY
			PointT topTempY_min, topTempY_max;
			PointT downTempY_min, downTempY_max;
			pcl::getMinMax3D(*topTempX, topTempY_min, topTempY_max);
			pcl::getMinMax3D(*downTempX, downTempY_min, downTempY_max);

			PointT p1, g1, p2, g2;
			p1.x = i; p1.y = topTempY_min.y; p1.z = topTempY_max.z;
			g1.x = i; g1.y = topTempY_max.y; g1.z = topTempY_max.z;
			p2.x = i; p2.y = downTempY_min.y; p2.z = downTempY_min.z;
			g2.x = i; g2.y = downTempY_max.y; g2.z = downTempY_min.z;
			if (abs(p1.y) < 10000 && abs(p1.z) < 10000 && abs(g1.y) < 10000 && abs(g1.z) < 10000) {
				generateLinePointCloud(p1, g1, paras.pointPitch, 255, allCloudFilled);
			}
			if (abs(p2.y) < 10000 && abs(p2.z) < 10000 && abs(g2.y) < 10000 && abs(g2.z) < 10000) {
				generateLinePointCloud(p2, g2, paras.pointPitch, 255, allCloudFilled);
			}
		}
	}
	simpleView("cloud Filled", allCloudFilled);
	pcl::io::savePLYFile("OutputData/6_AllPlanes.ply", *allCloudFilled);
	return (0);
}



void generateLinePointCloud(PointT pt1, PointT pt2, int pointPitch, int color, PointCloudT::Ptr output) {
	int numPoints = pcl::geometry::distance(pt1, pt2) * pointPitch;
	float ratioX = (pt1.x - pt2.x) / numPoints;
	float ratioY = (pt1.y - pt2.y) / numPoints;
	float ratioZ = (pt1.z - pt2.z) / numPoints;
	for (size_t i = 0; i <numPoints; i++) {
		PointT p;
		p.x = pt2.x + i * (ratioX);
		p.y = pt2.y + i * (ratioY);
		p.z = pt2.z + i * (ratioZ);
		p.rgba = color;
		output->points.push_back(p);
	}
}

void extendSmallPlaneToBigPlane(Plane& sourceP, Plane& targetP, int color, int pointPitch, PointCloudT::Ptr output) {
	Eigen::Vector3d normal = sourceP.getNormal();
	float slope = normal[1] / normal[0];
	float b1 = sourceP.leftUp().y - slope * sourceP.leftUp().x;
	float b2 = sourceP.rightUp().y - slope * sourceP.rightUp().x;
	float covered_slope = (targetP.rightUp().y - targetP.leftUp().y) / (targetP.rightUp().x - targetP.leftUp().x);
	float covered_b = targetP.rightUp().y - covered_slope * targetP.rightUp().x;
	Eigen::Matrix2f A;
	A << slope, -1, covered_slope, -1;
	Eigen::Vector2f B1, B2;
	B1 << -b1, -covered_b;
	B2 << -b2, -covered_b;
	Eigen::Vector2f X1, X2;
	X1 = A.colPivHouseholderQr().solve(B1);
	X2 = A.colPivHouseholderQr().solve(B2);
	PointT p1, q1, p2, q2; // four points at covered plane
	p1.x = X1[0]; p1.y = X1[1]; p1.z = sourceP.leftUp().z;
	q1.x = X1[0]; q1.y = X1[1]; q1.z = sourceP.leftDown().z;

	// mark: we need to make sure  X1-left and X2-right wont intersect
	if (isIntersect(p1, q1, sourceP.leftUp(), sourceP.leftDown())) swap(X1, X2);
	p1.x = X1[0]; p1.y = X1[1]; p1.z = sourceP.leftUp().z;
	q1.x = X1[0]; q1.y = X1[1]; q1.z = sourceP.leftDown().z;
	p2.x = X2[0]; p2.y = X2[1]; p2.z = sourceP.leftUp().z;
	q2.x = X2[0]; q2.y = X2[1]; q2.z = sourceP.leftDown().z;

	//cout << "p1 " << p1.x << " " << p1.y << " " << p1.z << endl;
	//cout << "p2 " << p2.x << " " << p2.y << " " << p2.z << endl;
	//cout << "q1 " << q1.x << " " << q1.y << " " << q1.z << endl;
	//cout << "q2 " << q2.x << " " << q2.y << " " << q2.z << endl;
	//cout << "leftUp " << sourceP.leftUp().x << " " << sourceP.leftUp().y << " " << sourceP.leftUp().z << endl;
	//cout << "rightUp " << sourceP.rightUp().x << " " << sourceP.rightUp().y << " " << sourceP.rightUp().z << endl;

	Plane all;
	Plane tmp_a(p1, q1, sourceP.leftUp(), sourceP.leftDown(), pointPitch, Color_Peach);
	Plane tmp_b(p2, q2, sourceP.rightUp(), sourceP.rightDown(), pointPitch, Color_Peach);
	Plane tmp_c(p1, p2, sourceP.leftUp(), sourceP.rightUp(), pointPitch, Color_Peach);
	Plane tmp_d(q1, q2, sourceP.leftDown(), sourceP.rightDown(), pointPitch, Color_Peach);
	sourceP.append(tmp_a); sourceP.append(tmp_b); sourceP.append(tmp_c); sourceP.append(tmp_d);

	// mark: remove the point which located within four points
	if (X1[0] > X2[0]) swap(X1[0], X2[0]);
	if (X1[1] > X2[1]) swap(X1[1], X2[1]);
	float zMin = sourceP.leftDown().z, zMax = sourceP.leftUp().z;
	float xMin = X1[0], xMax = X2[0];
	float yMin = X1[1], yMax = X2[1];
	targetP.removePointWithin(xMin, xMax, yMin, yMax, zMin, zMax);
}


// fixme: these insect are baed on 2 dimention - fix them in 3d dimention

bool onSegment(PointT p, PointT q, PointT r)
{
	// Given three colinear points p, q, r, the function checks if
	// point q lies on line segment 'pr'
	if (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
		q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y) &&
		q.z <= max(p.z, r.z) && q.z >= min(p.z, r.z))
		return true;
	return false;
}

float orientation(PointT p, PointT q, PointT r) {
	// To find orientation of ordered triplet (p, q, r).
	// The function returns following values
	// 0 --> p, q and r are colinear
	// 1 --> Clockwise
	// 2 --> Counterclockwise
	float val = (q.y - p.y) * (r.x - q.x) -
		(q.x - p.x) * (r.y - q.y);

	if (val == 0) return 0;  // colinear

	return (val > 0) ? 1 : 2; // clock or counterclock wise
}

bool isIntersect(PointT p1, PointT q1, PointT p2, PointT q2)
{
	// Find the four orientations needed for general and
	// special cases
	float o1 = orientation(p1, q1, p2);
	float o2 = orientation(p1, q1, q2);
	float o3 = orientation(p2, q2, p1);
	float o4 = orientation(p2, q2, q1);

	// General case
	if (o1 != o2 && o3 != o4)
		return true;

	// Special Cases
	// p1, q1 and p2 are colinear and p2 lies on segment p1q1
	if (o1 == 0 && onSegment(p1, p2, q1)) return true;

	// p1, q1 and q2 are colinear and q2 lies on segment p1q1
	if (o2 == 0 && onSegment(p1, q2, q1)) return true;

	// p2, q2 and p1 are colinear and p1 lies on segment p2q2
	if (o3 == 0 && onSegment(p2, p1, q2)) return true;

	// p2, q2 and q1 are colinear and q1 lies on segment p2q2
	if (o4 == 0 && onSegment(p2, q1, q2)) return true;

	return false; // Doesn't fall in any of the above cases
}

void importConfig(const YAML::Node& node, reconstructParas &para){
    YAML::Node RANSAC     = node["RANSAC"];
    YAML::Node Downsample = node["Downsampling"];
    YAML::Node Clustering = node["Clustering"];
    YAML::Node Combine    = node["Combine"];
    para.pointPitch       = node["pointPitch"].as<int>();
    para.minPlaneHeight   = node["minPlaneHeight"].as<float>();

    para.RANSAC_DistThreshold        = RANSAC["RANSAC_DistThreshold"].as<float>();
    para.RANSAC_MinInliers           = RANSAC["RANSAC_MinInliers"].as<float>();
    para.RANSAC_PlaneVectorThreshold = RANSAC["RANSAC_PlaneVectorThreshold"].as<float>();

    para.KSearch  = Downsample["KSearch"].as<int>();
    para.leafSize = Downsample["leafSize"].as<float>();

    para.MinSizeOfCluster    = Clustering["MinSizeOfCluster"].as<int>();
    para.NumberOfNeighbours  = Clustering["NumberOfNeighbours"].as<int>();
    para.SmoothnessThreshold = Clustering["SmoothnessThreshold"].as<int>();
    para.CurvatureThreshold  = Clustering["CurvatureThreshold"].as<int>();

    para.minimumEdgeDist      = Combine["minimumEdgeDist"].as<float>();
    para.minPlanesDist        = Combine["minPlanesDist"].as<float>();
    para.minAngle_normalDiff  = Combine["minAngle_normalDiff"].as<float>();
}