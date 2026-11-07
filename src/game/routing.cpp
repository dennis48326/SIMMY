// Port of game/routing.cpp (PenaltyRect, SpacePartition, Path: the router's rectangle A*) and
// game/irect.cpp (IRect). See spacepartition.hpp. The GC kept the vectors as STLport vectors of
// 0x14-byte rectangles, ints and 0x2C-byte nodes; here they are std::vectors of the same records,
// indexed the same way.
#include "simmy/types_gen.hpp"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------------------------------
// IRect (game/irect.cpp)

// 800A5A98
RET_UNKNOWN IRect::IsContained(IRect* outer) const {
  return bottom <= outer->bottom && outer->top <= top && outer->left <= left && right <= outer->right ? 1 : 0;
}

// 800A5AE4
// An empty rectangle inside the other gives nothing. The rectangle with the larger top (a on a tie)
// and the one with the larger left (b only when larger) decide the overlap.
RET_UNKNOWN IRect::Intersect(IRect* a, IRect* b) {
  auto zero = [this] {
    top = left = bottom = right = 0;
    return 0;
  };
  const bool aEmpty = !(a->top < a->bottom && a->left < a->right);
  if (aEmpty && a->IsContained(b)) return zero();
  const bool bEmpty = !(b->top < b->bottom && b->left < b->right);
  if (bEmpty && b->IsContained(a)) return zero();
  IRect* lowTop = b;
  IRect* highTop = a;
  if (a->top < b->top) lowTop = a, highTop = b;
  IRect* highLeft = a;
  IRect* lowLeft = b;
  if (a->left < b->left) highLeft = b, lowLeft = a;
  if (highTop->top < lowTop->bottom && highLeft->left < lowLeft->right) {
    const int l = highLeft->left, t = highTop->top;
    const int r = std::min(highLeft->right, lowLeft->right);
    const int bt = std::min(highTop->bottom, lowTop->bottom);
    left = l, top = t, right = r, bottom = bt;
    return 1;
  }
  return zero();
}

// ---------------------------------------------------------------------------------------------------
// PenaltyRect

// 8013896C
PenaltyRect::PenaltyRect(IRect* r, int pen) {
  top = r->top, left = r->left, bottom = r->bottom, right = r->right;
  penalty = pen;
}

// 80138998
PenaltyRect::PenaltyRect(int l, int t, int r, int b, int pen) {
  left = l, top = t, right = r, bottom = b;
  penalty = pen;
}

namespace {
bool Overlaps(const IRect& a, const IRect& b) {
  return b.top < a.bottom && a.top < b.bottom && b.left < a.right && a.left < b.right;
}
float Dist(int dx, int dy) {
  const float fx = static_cast<float>(dx), fy = static_cast<float>(dy);
  return std::sqrt(fx * fx + fy * fy);
}
}  // namespace

// 801389B4
int FindIntersectingRect(IRect* r, std::vector<PenaltyRect>* rects) {
  for (size_t i = 0; i < rects->size(); ++i)
    if (Overlaps(*r, (*rects)[i])) return static_cast<int>(i);
  return -1;
}

// 80139C04
int IsRectInside(IRect* inner, IRect* outer) {
  if (!outer) return 1;
  return outer->left <= inner->left && inner->right <= outer->right && outer->top <= inner->top &&
                 inner->bottom <= outer->bottom
             ? 1
             : 0;
}

// 8013BA9C
bool EvalPoint(IPoint a, IPoint b, IPoint c, IPoint* out, float* best) {
  const float d = Dist(a.x - b.x, a.y - b.y) + Dist(b.x - c.x, b.y - c.y);
  const float old = *best;
  if (d < old) {
    *best = d;
    *out = b;
  }
  return d < old;
}

// ---------------------------------------------------------------------------------------------------
// SpacePartition

SpacePartition Path::fSpacePartition;

// 80138C20
SpacePartition::SpacePartition() {}

// 80138A3C
void SpacePartition::FindInterfaceRect(int a, int b, IRect* out) {
  ASTNode* na = GetNode(a);
  ASTNode* nb = GetNode(b);
  if (!na || !nb) {
    out->top = out->left = out->bottom = out->right = 0;
    return;
  }
  FindInterfaceRect(na, nb, out);
}

// 80138ABC
// Both nodes' rectangles grown by one each way, overlapped: the strip along their shared edge.
RET_UNKNOWN SpacePartition::FindInterfaceRect(ASTNode* a, ASTNode* b, IRect* out) {
  const PenaltyRect& ra = m_free[static_cast<size_t>(a->free)];
  const PenaltyRect& rb = m_free[static_cast<size_t>(b->free)];
  out->top = ra.top - 1, out->left = ra.left - 1, out->bottom = ra.bottom + 1, out->right = ra.right + 1;
  IRect other;
  other.top = rb.top - 1, other.left = rb.left - 1, other.bottom = rb.bottom + 1, other.right = rb.right + 1;
  return out->Intersect(&other, out);
}

// 80138B98
IPoint SpacePartition::FindInterfacePoint(ASTNode* a, ASTNode* b) {
  IRect r;
  FindInterfaceRect(a, b, &r);
  IPoint p;
  p.x = (r.right + r.left) / 2;
  p.y = (r.bottom + r.top) / 2;
  return p;
}

// 80138C94
int SpacePartition::GetIntersectingFreeRect(IRect* r) {
  for (size_t i = 0; i < m_free.size(); ++i)
    if (Overlaps(*r, m_free[i])) return static_cast<int>(i);
  return -1;
}

// 80138D18
// A blocking rectangle overlapping `r` (the objects first, then the room's), else the last one that
// overlaps it, else null.
PenaltyRect* SpacePartition::GetIntersectingPartitionRect(IRect* r) {
  PenaltyRect* found = nullptr;
  for (PenaltyRect& p : *m_obstacles) {
    if (!Overlaps(*r, p)) continue;
    found = &p;
    if (p.penalty == PenaltyRect::kBlocked) return &p;
  }
  if (!m_partition) return found;
  for (PenaltyRect& p : *m_partition) {
    if (!Overlaps(*r, p)) continue;
    found = &p;
    if (p.penalty == PenaltyRect::kBlocked) return &p;
  }
  return found;
}

// 80138E30
ASTNode* SpacePartition::GetNode(int n) {
  if (n < 0 || static_cast<size_t>(n) >= m_nodes.size()) return nullptr;
  return &m_nodes[static_cast<size_t>(n)];
}

// 80138E70
int SpacePartition::GetNodeRectangle(int n, IRect* out) {
  if (n == -1) return 0;
  ASTNode* node = GetNode(n);
  if (!node || node->free < 0 || static_cast<size_t>(node->free) >= m_free.size()) return 0;
  *out = m_free[static_cast<size_t>(node->free)];
  return 1;
}

// 80138F1C
bool SpacePartition::IsSpatialNode(int n) { return 2u < static_cast<unsigned int>(n + 1); }

// 80138F30
// The straight distance from where the path enters the node to its goal's centre.
double SpacePartition::EstimateDistanceToGoal(int n) {
  if (static_cast<unsigned int>(n) < 2) return 0.0;
  ASTNode* node = GetNode(n);
  if (!node || node->goal < 0 || static_cast<size_t>(node->goal) >= m_params->goals->size() || node->free < 0 ||
      static_cast<size_t>(node->free) >= m_free.size())
    return 0.0;
  const PenaltyRect& g = (*m_params->goals)[static_cast<size_t>(node->goal)];
  return Dist((g.left + g.right) / 2 - node->pt.x, (g.top + g.bottom) / 2 - node->pt.y);
}

// 80139070
// From the start to a goal's entry node: the goal's penalty, at the start. Between spatial nodes: from
// where the path enters `from` to the middle of the edge it shares with `to` (or the same point inside
// one rectangle), plus one, times the two rectangles' mean penalty plus one. To the end node: 0.
// PC: *pt is `from`'s point where the GC left it unset (the start to a node without a goal, the end).
double SpacePartition::MeasureDistance(int from, int to, IPoint* pt) {
  ASTNode* a = GetNode(from);
  ASTNode* b = GetNode(to);
  if (!a || !b) return 0.0;
  *pt = a->pt;
  if (from == 0) {
    if (b->goal >= 0 && static_cast<size_t>(b->goal) < m_params->goals->size())
      return static_cast<float>((*m_params->goals)[static_cast<size_t>(b->goal)].penalty);
    return 0.0;
  }
  if (to == 1) return 0.0;
  IPoint p = a->free == b->free ? a->pt : FindInterfacePoint(a, b);
  *pt = p;
  const float d = Dist(p.x - a->pt.x, p.y - a->pt.y);
  return (d + 1.0f) * static_cast<float>((a->penalty + b->penalty + 2) >> 1);
}

// 80139250
void SpacePartition::GetTerminals(int* start, int* goal) {
  *start = 0;
  *goal = 1;
}

// 80139264
// The start leads to every goal's entry node; the end node leads nowhere; a spatial node to its
// neighbours (BuildSpatialSuccessorList), to the end when it is its goal's own rectangle, and an entry
// node to its goal's node when that shares its rectangle.
int SpacePartition::CountSuccessors(int n) {
  ASTNode* node = GetNode(n);
  if (!node) return 0;
  if (node->succCount == -1) {
    node->succCount = 0;
    node->succStart = static_cast<int>(m_successors.size());
    if (n == 0) {
      for (int e : m_entryNodes) {
        if (e == -1) continue;
        m_successors.push_back(e);
        ++node->succCount;
      }
    } else if (n != 1) {
      BuildSpatialSuccessorList(n);
      node = GetNode(n);
      bool isGoalNode = false;
      for (int g : m_goalNodes)
        if (g == n) isGoalNode = true;
      if (isGoalNode) {
        ++node->succCount;
        m_successors.push_back(1);
      }
      bool isEntry = false;
      for (int e : m_entryNodes)
        if (e == n) isEntry = true;
      if (isEntry) {
        for (int g : m_goalNodes) {
          if (g == -1) continue;
          ASTNode* gn = GetNode(g);
          if (!gn || gn->free != node->free || gn->goal != node->goal) continue;
          ++node->succCount;
          m_successors.push_back(g);
        }
      }
    }
  }
  return node->succCount;
}

// 801397D0
int SpacePartition::GetNthSuccessor(int n, int i) {
  const int count = CountSuccessors(n);
  ASTNode* node = GetNode(n);
  if (i < 0 || i >= count || !node) return -1;
  return m_successors[static_cast<size_t>(node->succStart + i)];
}

// 80139844
void SpacePartition::Clear() {
  m_free.clear();
  m_nodes.clear();
  m_goalNodes.clear();
  m_entryNodes.clear();
  m_successors.clear();
}

// 801398BC
void SpacePartition::Deallocate() {
  std::vector<PenaltyRect>().swap(m_free);
  std::vector<ASTNode>().swap(m_nodes);
  std::vector<int>().swap(m_goalNodes);
  std::vector<int>().swap(m_entryNodes);
  std::vector<int>().swap(m_successors);
}

// 80139C60
// Grows `r` (which must not touch a blocking rectangle or a known free one) a side at a time, each by
// a step that halves whenever growing would touch a blocking or free rectangle, leave the partition
// rectangle it started in, or (limitSize) pass maxSize; stops when no side can grow (or after 200
// moves). Then (limitAspect) cuts it to maxAspect:1 round the middle of what it was. 1: not empty.
int SpacePartition::ExpandRect(PenaltyRect* r) {
  const int maxSize = m_params->maxSize;
  const int startTop = r->top, startLeft = r->left, startBottom = r->bottom, startRight = r->right;
  if (m_params->limitSize != 0 && !(r->right - r->left <= maxSize && r->bottom - r->top <= maxSize)) return 0;
  PenaltyRect* container = GetIntersectingPartitionRect(r);
  if (container && container->penalty == PenaltyRect::kBlocked) return 0;
  if (GetIntersectingFreeRect(r) != -1) return 0;
  int first = 0x20;
  if (m_params->limitSize != 0) {
    first = maxSize / 4;
    if (first < 1) first = 1;
  }
  int steps[4] = {-first, -first, first, first};  // top, left, bottom, right
  int* side[4] = {&r->top, &r->left, &r->bottom, &r->right};
  int budget = 200, zeros = 0;
  unsigned int s = 0;
  do {
    const int step = steps[s];
    bool advance = false;
    if (step == 0) {
      ++zeros;
      advance = true;
    } else {
      const int old = *side[s];
      zeros = 0;
      *side[s] = old + step;
      if (m_params->limitSize == 0 || (r->right - r->left <= maxSize && r->bottom - r->top <= maxSize)) {
        PenaltyRect* p = GetIntersectingPartitionRect(r);
        if (p == container && (!p || p->penalty != PenaltyRect::kBlocked) && GetIntersectingFreeRect(r) == -1 &&
            IsRectInside(r, container)) {
          advance = true;
        } else {
          *side[s] -= steps[s];
          steps[s] /= 2;
        }
      } else {
        *side[s] = old;
        steps[s] /= 2;
      }
    }
    if (advance) {
      --budget;
      s = (s + 1) & 3;
      if (budget == 0) break;
    }
  } while (zeros < 4);
  if (m_params->limitAspect != 0) {
    const int aspect = m_params->maxAspect;
    int top = r->top, left = r->left, bottom = r->bottom, right = r->right;
    const int w = right - left, h = bottom - top;
    if (h < w) {
      if (static_cast<float>(aspect) < static_cast<float>(w) / static_cast<float>(h)) {
        const int nw = h * aspect;
        const int off = (-(nw / 2) - left) + (startRight + startLeft) / 2;
        const int nl = left + off;
        int nr = left + nw + off;
        if (nl < left) {
          nr += left - nl;
        } else {
          left = nl;
          if (r->right < nr) {
            left = nl + (r->right - nr);
            nr += r->right - nr;
          }
        }
        right = nr;
      }
    } else if (static_cast<float>(aspect) < static_cast<float>(h) / static_cast<float>(w)) {
      const int nh = w * aspect;
      const int off = (-(nh / 2) - top) + (startBottom + startTop) / 2;
      int nb = top + nh + off;
      int nt = top + off;
      if (nt < top) {
        nb += top - nt;
        nt = top;
      } else if (r->bottom < nb) {
        const int d = r->bottom - nb;
        nb += d;
        nt += d;
      }
      top = nt;
      bottom = nb;
    }
    r->top = top, r->left = left, r->bottom = bottom, r->right = right;
  }
  return r->left < r->right && r->top < r->bottom ? 1 : 0;
}

// 8013A130
// Walks a one-unit probe round the outside of node n's rectangle (along the top from its left, down
// the right, back along the bottom, up the left; corners left out): a blocking rectangle there is
// skipped; free space is the known free rectangle there or a new one grown from the probe (its penalty
// the partition rectangle's there, else the default), whose node for n's goal (found or made) becomes
// a successor. The probe then jumps past what it found.
void SpacePartition::BuildSpatialSuccessorList(int n) {
  ASTNode* node = GetNode(n);
  if (!node) return;
  const PenaltyRect fr = m_free[static_cast<size_t>(node->free)];
  const int goal = node->goal;
  PenaltyRect probe(const_cast<PenaltyRect*>(&fr), 0);
  probe.bottom = probe.top;
  probe.right = probe.left + 1;
  probe.top = probe.top - 1;
  int s = 0;
  do {
    int found = -1;
    IRect r;
    PenaltyRect* pr = GetIntersectingPartitionRect(&probe);
    if (!pr || pr->penalty != PenaltyRect::kBlocked) {
      int fi = GetIntersectingFreeRect(&probe);
      bool ok = true;
      if (fi == -1) {
        PenaltyRect grown = probe;
        if (ExpandRect(&grown) == 0) {
          // GC: tried again, then went on with free rectangle -1 (reading before the vector); ExpandRect
          // cannot fail on a probe that touches nothing. PC: the probe is skipped.
          ExpandRect(&grown);
          ok = false;
        } else {
          grown.penalty = pr ? pr->penalty : m_params->defaultPenalty;
          m_free.push_back(grown);
          fi = static_cast<int>(m_free.size()) - 1;
        }
      }
      if (ok) {
        for (size_t i = 0; i < m_nodes.size(); ++i) {
          if (m_nodes[i].goal == goal && m_nodes[i].free == fi) {
            found = static_cast<int>(i);
            break;
          }
        }
        if (found == -1) {
          ASTNode a;
          a.goal = goal;
          a.free = fi;
          a.succCount = -1;
          a.succStart = -1;
          a.penalty = m_free[static_cast<size_t>(fi)].penalty;
          m_nodes.push_back(a);
          found = static_cast<int>(m_nodes.size()) - 1;
          node = GetNode(n);
        }
        r = m_free[static_cast<size_t>(m_nodes[static_cast<size_t>(found)].free)];
      } else {
        r = probe;
      }
    } else {
      r = *pr;
    }
    if (found != -1) {
      m_successors.push_back(found);
      ++node->succCount;
    }
    const int oldTop = probe.top, oldLeft = probe.left;
    switch (s % 4) {
      case 0:  // along the top, rightward
        if (fr.right < probe.right || fr.right <= r.right) {
          const int d = fr.right - probe.left;
          probe.top += 1;
          probe.bottom += 1;
          probe.left = fr.right;
          probe.right += d;
          ++s;
        } else {
          probe.left = r.right;
          probe.right += r.right - oldLeft;
        }
        break;
      case 1:  // down the right
        if (fr.bottom < probe.bottom || fr.bottom <= r.bottom) {
          ++s;
          probe.bottom += fr.bottom - probe.top;
          probe.left -= 1;
          probe.top = fr.bottom;
          probe.right -= 1;
        } else {
          probe.top = r.bottom;
          probe.bottom += r.bottom - oldTop;
        }
        break;
      case 2:  // along the bottom, leftward
        if (probe.left < fr.left || r.left <= fr.left) {
          const int d = (fr.left - probe.left) - 1;
          probe.bottom -= 1;
          probe.top -= 1;
          probe.left += d;
          probe.right += d;
          ++s;
        } else {
          const int d = (r.left - probe.left) - 1;
          probe.right += d;
          probe.left += d;
        }
        break;
      default:  // up the left
        if (probe.top < fr.top || r.top <= fr.top) {
          const int d = (fr.top - probe.top) - 1;
          probe.bottom += d;
          probe.top += d;
          probe.left += 1;
          probe.right += 1;
          ++s;
        } else {
          const int d = (r.top - probe.top) - 1;
          probe.bottom += d;
          probe.top += d;
        }
        break;
    }
  } while (s < 4);
}

// 8013C0EC
// Node 0: the start in the free rectangle grown round it; node 1: the end. Per goal not on a blocking
// rectangle: its node in its own free rectangle (found, or grown from the goal; none if that fails) and
// its entry node in the start's rectangle with the goal's penalty.
void SpacePartition::Init(RoutingParams* params) {
  m_params = params;
  m_obstacles = params->obstacles;
  m_partition = params->partition;
  Clear();
  PenaltyRect start(params->start.x, params->start.y, params->start.x + 1, params->start.y + 1, 0);
  ExpandRect(&start);
  m_free.push_back(start);
  ASTNode s;
  s.free = static_cast<int>(m_free.size()) - 1;
  s.penalty = 0;
  m_nodes.push_back(s);
  ASTNode end;
  end.penalty = 0;
  m_nodes.push_back(end);
  std::vector<PenaltyRect>& goals = *params->goals;
  for (size_t gi = 0; gi < goals.size(); ++gi) {
    PenaltyRect* g = &goals[gi];
    m_goalNodes.push_back(-1);
    m_entryNodes.push_back(-1);
    PenaltyRect* pr = GetIntersectingPartitionRect(g);
    if (pr && pr->penalty == PenaltyRect::kBlocked) continue;
    int fi = GetIntersectingFreeRect(g);
    if (fi == -1) {
      PenaltyRect grown(g, pr ? pr->penalty : 0);
      if (ExpandRect(&grown) == 0) continue;
      m_free.push_back(grown);
      fi = static_cast<int>(m_free.size()) - 1;
    }
    ASTNode a;
    a.goal = static_cast<int>(gi);
    a.free = fi;
    a.penalty = m_free[static_cast<size_t>(fi)].penalty;
    m_nodes.push_back(a);
    m_goalNodes.back() = static_cast<int>(m_nodes.size()) - 1;
    ASTNode b;
    b.goal = static_cast<int>(gi);
    b.free = m_nodes[0].free;
    b.penalty = g->penalty;
    m_nodes.push_back(b);
    m_entryNodes.back() = static_cast<int>(m_nodes.size()) - 1;
  }
}

// ---------------------------------------------------------------------------------------------------
// Path

// 8013ACAC
RET_UNKNOWN Path::InitAST() {
  m_open.clear();
  m_closed.clear();
  fSpacePartition.GetTerminals(&m_start, &m_goal);
  ASTNode* s = fSpacePartition.GetNode(m_start);
  s->pt = m_params->start;
  const float h = static_cast<float>(fSpacePartition.EstimateDistanceToGoal(m_start));
  s->parent = -1;
  s->g = 0.0f;
  s->f = h;
  s->h = h;
  m_current = m_start;
  m_open.push_back(m_start);
  return 1;
}

// 8013AE78
// At the end node: the chain back to the start, the spatial nodes along it (one per rectangle) and
// their way points, then (unless noGoalCentre) the centre of the goal reached. Else the current node's
// successors are scored (relaxed when cheaper, reopened from the closed list), it is closed, and the
// cheapest open node is next; 1 when nothing is left open.
RET_UNKNOWN Path::OpenANode() {
  SpacePartition& sp = fSpacePartition;
  if (m_current == m_goal) {
    while (m_current != -1) {
      m_chain.push_back(m_current);
      ASTNode* node = sp.GetNode(m_current);
      m_current = node->parent;
      if (m_goalRect == -1 && node->goal != -1) m_goalRect = node->goal;
    }
    int prev = -1;
    for (int i = static_cast<int>(m_chain.size()) - 1; i >= 0; --i) {
      const int n = m_chain[static_cast<size_t>(i)];
      ASTNode* node = sp.GetNode(n);
      if (node->free != -1 && node->free != prev) {
        m_pathNodes.push_back(n);
        prev = node->free;
      }
    }
    for (int n : m_pathNodes) m_points.push_back(sp.GetNode(n)->pt);
    if (!m_pathNodes.empty() && m_params->noGoalCentre == 0 && m_chain.size() > 1) {
      ASTNode* last = sp.GetNode(m_chain[1]);
      const PenaltyRect& g = (*m_params->goals)[static_cast<size_t>(last->goal)];
      IPoint c;
      c.x = (g.left + g.right) / 2;
      c.y = (g.top + g.bottom) / 2;
      m_points.push_back(c);
    }
    return 1;
  }
  const int count = sp.CountSuccessors(m_current);
  for (int i = 0; i < count; ++i) {
    const int s = sp.GetNthSuccessor(m_current, i);
    IPoint pt;
    const double cost = sp.MeasureDistance(m_current, s, &pt);
    const float g = static_cast<float>(static_cast<double>(sp.GetNode(m_current)->g) + cost);
    ASTNode* node = sp.GetNode(s);
    bool closed = false;
    for (int c : m_closed)
      if (c == s) closed = true;
    if (closed) {
      if (!(g < node->g)) continue;
      for (size_t k = 0; k < m_closed.size(); ++k)
        if (m_closed[k] == s) {
          m_closed.erase(m_closed.begin() + static_cast<long>(k));
          break;
        }
    }
    bool open = false;
    for (int o : m_open)
      if (o == s) open = true;
    if (open) {
      if (node->g <= g) continue;
    } else {
      m_open.push_back(s);
    }
    node->g = g;
    node->pt = pt;
    const float h = static_cast<float>(sp.EstimateDistanceToGoal(s));
    node->h = h;
    node->f = node->g + h;
    node->parent = m_current;
  }
  for (size_t k = 0; k < m_open.size(); ++k)
    if (m_open[k] == m_current) {
      m_open.erase(m_open.begin() + static_cast<long>(k));
      break;
    }
  m_closed.push_back(m_current);
  if (m_open.empty()) return 1;
  m_current = static_cast<int>(FindSmallestOpenNode());
  return 0;
}

// 8013BC20
// Each inner way point moves along the edge it crosses (staying `step` inside it) when that shortens
// the way through it; the step starts at 100 and shrinks (100 / (level + 1)^2) whenever a pass moves
// nothing. False: something moved (call again).
bool Path::DoOneSmooth() {
  const int n = static_cast<int>(m_pathNodes.size());
  if (n < 2) return true;
  const int npts = static_cast<int>(m_points.size());
  int moved = 0;
  bool done;
  int step;
  do {
    step = 100 / ((m_smoothLevel + 1) * (m_smoothLevel + 1));
    if (step < 1) step = 1;
    for (int i = 1; i < n; ++i) {
      IRect edge;
      fSpacePartition.FindInterfaceRect(m_pathNodes[static_cast<size_t>(i - 1)], m_pathNodes[static_cast<size_t>(i)], &edge);
      const IPoint prev = m_points[static_cast<size_t>(i - 1)];
      const IPoint cur = m_points[static_cast<size_t>(i)];
      const IPoint next = i + 1 < npts ? m_points[static_cast<size_t>(i + 1)] : cur;
      float best = Dist(prev.x - cur.x, prev.y - cur.y) + Dist(cur.x - next.x, cur.y - next.y);
      IPoint bestPt = cur;
      if (2 < edge.right - edge.left) {
        IPoint c = cur;
        if (c.x < edge.right - step) {
          c.x += step;
          moved += EvalPoint(prev, c, next, &bestPt, &best) ? 1 : 0;
        }
        c = cur;
        if (edge.left + step < c.x) {
          c.x -= step;
          moved += EvalPoint(prev, c, next, &bestPt, &best) ? 1 : 0;
        }
      }
      if (2 < edge.bottom - edge.top) {
        IPoint c = cur;
        if (c.y < edge.bottom - step) {
          c.y += step;
          moved += EvalPoint(prev, c, next, &bestPt, &best) ? 1 : 0;
        }
        c = cur;
        if (edge.top + step < c.y) {
          c.y -= step;
          moved += EvalPoint(prev, c, next, &bestPt, &best) ? 1 : 0;
        }
      }
      m_points[static_cast<size_t>(i)] = bestPt;
    }
    done = moved == 0;
    if (!done) break;
    ++m_smoothLevel;
  } while (1 < step);
  return done;
}

// 8013D6AC
void Path::InitPath(RoutingParams* params) {
  m_params = params;
  fSpacePartition.Init(params);
  m_chain.clear();
  m_pathNodes.clear();
  m_points.clear();
  m_steps = 0;
  m_goalRect = -1;
  m_state = 0;
  m_smoothLevel = 0;
  if (InitAST() != 0) m_state = 1;
}

// 8013D750
RET_UNKNOWN Path::FindSmallestOpenNode() {
  size_t best = 0;
  float f = fSpacePartition.GetNode(m_open[0])->f;
  for (size_t i = 1; i < m_open.size(); ++i) {
    const float fi = fSpacePartition.GetNode(m_open[i])->f;
    if (fi < f) {
      best = i;
      f = fi;
    }
  }
  return m_open[best];
}

// 8013D7FC
void Path::DeallocateAllPathData() { fSpacePartition.Deallocate(); }
