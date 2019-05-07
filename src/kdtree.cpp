#include "kdtree.hpp"
#include "model.hpp"

#include <glm/gtc/random.hpp>
#include <glm/gtx/intersect.hpp>

#include <glm/gtx/io.hpp>

#include <algorithm>
#include <iostream>

float KDTree::triMax(Triangle &t, id_t axis) {
    float max = vertices[t.fst].Position[axis];
    max = vertices[t.snd].Position[axis] > max ? vertices[t.snd].Position[axis] : max;
    return vertices[t.trd].Position[axis] > max ? vertices[t.trd].Position[axis] : max;
}

float KDTree::triMin(Triangle &t, id_t axis) {
    float min = vertices[t.fst].Position[axis];
    min = vertices[t.snd].Position[axis] < min ? vertices[t.snd].Position[axis] : min;
    return vertices[t.trd].Position[axis] < min ? vertices[t.trd].Position[axis] : min;
}

bool KDTree::inLeft(Triangle &t, float max, id_t axis) {
    return vertices[t.fst].Position[axis] <= max || vertices[t.snd].Position[axis] <= max ||
           vertices[t.trd].Position[axis] <= max;
}
bool KDTree::inRight(Triangle &t, float min, id_t axis) {
    return vertices[t.fst].Position[axis] >= min || vertices[t.snd].Position[axis] >= min ||
           vertices[t.trd].Position[axis] >= min;
}
bool lessThan(glm::vec3 &compare, glm::vec3 &to) { return compare.x < to.x || compare.y < to.y || compare.z < to.z; }
bool greaterThan(glm::vec3 &compare, glm::vec3 &to) { return compare.x > to.x || compare.y > to.y || compare.z > to.z; }

KDTree::KDTree(Model &model) : KDTree(model.meshes) {}

KDTree::KDTree(std::vector<Mesh> &meshes) : minCoords(FLT_MAX, FLT_MAX, FLT_MAX), maxCoords(FLT_MIN, FLT_MIN, FLT_MIN) {
    size_t verticesCount = 0;
    size_t indicesCount = 0;

    for (auto &mesh : meshes) {
        verticesCount += mesh.vertices.size();
        indicesCount += mesh.indices.size();
    }

    vertices.resize(verticesCount);
    materials.resize(verticesCount);
    std::vector<Triangle> triangles(indicesCount / 3);

    indicesCount = 0;
    verticesCount = 0;
    for (auto &mesh : meshes) {
        auto &indices = mesh.indices;
        for (unsigned i = 0; i < indices.size(); i += 3, indicesCount++) {
            triangles[indicesCount].fst = indices[i + 0] + verticesCount;
            triangles[indicesCount].snd = indices[i + 1] + verticesCount;
            triangles[indicesCount].trd = indices[i + 2] + verticesCount;
        }
        for (unsigned i = 0; i < mesh.vertices.size(); i++) {
            materials[verticesCount] = &mesh;
            vertices[verticesCount++] = mesh.vertices[i];
            for (unsigned j = 0; j < 3; j++) {
                minCoords[j] =
                    mesh.vertices[i].Position[j] < minCoords[j] ? mesh.vertices[i].Position[j] : minCoords[j];
                maxCoords[j] =
                    mesh.vertices[i].Position[j] > maxCoords[j] ? mesh.vertices[i].Position[j] : maxCoords[j];
            }
        }
    }
    triangles.resize(indicesCount);

    nodes.push_back(KDTree::KDNode());
    nodes[0] = build(triangles, maxCoords, minCoords);
    std::cout << "Triangles in scene: " << triangles.size() << "\n";

    minCoords -= 0.0001f;
    maxCoords += 0.0001f;
}

KDTree::KDNode::Split KDTree::findSplit(std::vector<Triangle> &tris, glm::vec3 &max, glm::vec3 &min) {
    id_t bestAxis = 3; // if 3 is returned then there's no point in splitting
    float bestSplit = 0.f;
    float bestCost = float(tris.size());

    for (id_t axis = 0; axis < 3; axis++) {
        for (float ratio = 0.01f; ratio < 1.0f; ratio += 0.01f) {
            float split = min[axis] + ratio * (max[axis] - min[axis]);
            float cost = 0.f;
            size_t leftCount = 0;
            size_t rightCount = 0;

            for (auto &tri : tris) {
                if (inLeft(tri, split, axis)) {
                    cost += ratio;
                    leftCount++;
                }
                if (inRight(tri, split, axis)) {
                    cost += (1.f - ratio);
                    rightCount++;
                }
            }
            if (leftCount < tris.size() && rightCount < tris.size() && cost < bestCost) {
                bestAxis = axis;
                bestSplit = split;
                bestCost = cost;
            }
        }
    }

    return {bestAxis, bestSplit};
}

KDTree::KDNode KDTree::build(std::vector<Triangle> &tris, glm::vec3 &max, glm::vec3 &min) {
    KDTree::KDNode node;
    if (tris.size() <= 2 || (node.split = findSplit(tris, max, min)).axis == 3) {
        node.isLeaf = true;
        node.triangles = tris;
        return node;
    }
    node.isLeaf = false;
    node.child = nodes.size();
    nodes.resize(nodes.size() + 2);
    {
        glm::vec3 min(FLT_MAX, FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        std::vector<Triangle> childTriangles;
        for (auto &tri : tris) {
            if (inLeft(tri, node.split.position, node.split.axis)) {
                childTriangles.push_back(tri);

                min[node.split.axis] = std::min(min[node.split.axis], triMin(tri, node.split.axis));
                max[node.split.axis] = std::max(max[node.split.axis], triMax(tri, node.split.axis));
                id_t ax = (node.split.axis + 1) % 3;
                min[ax] = std::min(min[ax], triMin(tri, ax));
                max[ax] = std::max(max[ax], triMax(tri, ax));
                ax = (ax + 1) % 3;
                min[ax] = std::min(min[ax], triMin(tri, ax));
                max[ax] = std::max(max[ax], triMax(tri, ax));
            }
        }

        nodes[node.child] = build(childTriangles, max, min);
    }
    {
        glm::vec3 min(FLT_MAX, FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        std::vector<Triangle> childTriangles;
        for (auto &tri : tris) {
            if (inRight(tri, node.split.position, node.split.axis)) {
                childTriangles.push_back(tri);

                min[node.split.axis] = std::min(min[node.split.axis], triMin(tri, node.split.axis));
                max[node.split.axis] = std::max(max[node.split.axis], triMax(tri, node.split.axis));
                id_t ax = (node.split.axis + 1) % 3;
                min[ax] = std::min(min[ax], triMin(tri, ax));
                max[ax] = std::max(max[ax], triMax(tri, ax));
                ax = (ax + 1) % 3;
                min[ax] = std::min(min[ax], triMin(tri, ax));
                max[ax] = std::max(max[ax], triMax(tri, ax));
            }
        }
        nodes[node.child + 1] = build(childTriangles, max, min);
    }

    return node;
}

std::pair<float, float> intersectRayBox(const glm::vec3 &origin, const glm::vec3 &dir, glm::vec3 &max, glm::vec3 &min) {
    float dirinvx = 1.f / dir.x;
    float dirinvy = 1.f / dir.y;
    float dirinvz = 1.f / dir.z;
    float txmin = (min.x - origin.x) * dirinvx;
    float txmax = (max.x - origin.x) * dirinvx;
    float tymin = (min.y - origin.y) * dirinvy;
    float tymax = (max.y - origin.y) * dirinvy;
    float tzmin = (min.z - origin.z) * dirinvz;
    float tzmax = (max.z - origin.z) * dirinvz;
    return {std::max(std::max(std::min(txmin, txmax), std::min(tymin, tymax)), std::min(tzmin, tzmax)),
            std::min(std::min(std::max(txmin, txmax), std::max(tymin, tymax)), std::max(tzmin, tzmax))};
}
bool KDTree::intersectRay(const glm::vec3 &origin, const glm::vec3 &dir, glm::vec3 &baryPosition, Triangle &triangle) {
    auto distance = intersectRayBox(origin, dir, maxCoords, minCoords);
    if (distance.second < 0 || distance.second < distance.first)
        return false;
    return intersectRayNode(origin, dir, baryPosition, triangle, nodes[0], distance.first, distance.second);
}
bool KDTree::intersectRayTriangle(const glm::vec3 &origin, const glm::vec3 &dir, glm::vec3 &baryPosition,
                                  const Triangle &tri) {
    return glm::intersectRayTriangle(origin, dir, vertices[tri.fst].Position, vertices[tri.snd].Position,
                                     vertices[tri.trd].Position, baryPosition);
}
bool KDTree::intersectRayNode(const glm::vec3 &origin, const glm::vec3 &dir, glm::vec3 &baryPosition,
                              Triangle &triangle, KDTree::KDNode &node, float tmin, float tmax) {
    if (node.isLeaf) {
        glm::vec3 bPos;
        bool ret = false;
        int i = 0;
        for (auto &tri : node.triangles) {
            if (intersectRayTriangle(origin, dir, bPos, tri) && bPos.z < tmax) {
                baryPosition = bPos;
                tmax = bPos.z;
                triangle = tri;
                ret = true;
            }
        }
        return ret;
    }

    float tsplit = (node.split.position - origin[node.split.axis]) / dir[node.split.axis];
    bool belowFirst = (origin[node.split.axis] < node.split.position) ||
                      (origin[node.split.axis] == node.split.position && dir[node.split.axis] <= 0);

    KDTree::KDNode &first = belowFirst ? nodes[node.child] : nodes[node.child + 1];
    KDTree::KDNode &second = belowFirst ? nodes[node.child + 1] : nodes[node.child];

    if (tsplit >= tmax || tsplit < 0)
        return intersectRayNode(origin, dir, baryPosition, triangle, first, tmin, tmax);
    else if (tsplit <= tmin)
        return intersectRayNode(origin, dir, baryPosition, triangle, second, tmin, tmax);
    else
        return intersectRayNode(origin, dir, baryPosition, triangle, first, tmin, tsplit) ||
               intersectRayNode(origin, dir, baryPosition, triangle, second, tsplit, tmax);
}
// dummy function
bool KDTree::intersectShadowRay(const glm::vec3 &origin, const glm::vec3 &dir) {
    glm::vec3 baryPos;
    Triangle triangle;
    return intersectRay(origin, dir, baryPos, triangle);
}