/*
 * crack_generation.c
 * 初期き裂及び、合体後き裂のき裂面点群を生成するプログラム
 * プログラムを回すのに必要な情報は、
 * き裂前縁の点の座標を記した.nodeファイル
 * き裂面の奥行き方向（き裂前縁点群の始点を右、終点を左として、
 * 上向きの半円を描くようなき裂とした時の奥行き方向）を記したベクトルファイル
 * き裂を挿入するメッシュの表面パッチデータ.patch
 * 積層距離や前縁点間距離等のパラメータを記したファイル
 * である。
 * 出力は生成したき裂面上の点群ファイル.nodeと
 * き裂面についてのフラグ.nd_crack、
 * き裂表面についてのフラグ.nd_surf、
 * き裂前年についてのフラグ.nd_crack_front、
 * き裂面に垂直な方向のベクトルを示した.nd_normal
 * である
 */




#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<assert.h>

#define NUMBER_OF_CRACKFRONT_POINTS 10000
#define NUMBER_OF_CRACK_POINTS 1000000
#define NUMBER_OF_PATCHS 1000000
#define NUMBER_OF_LAMINATION_POINTS 1000000
#define NUMBER_OF_TRIANGLE_VERTEXS 3
#define NUMBER_OF_OUTER_LAYERS 2
#define NUMBER_OF_INNER_LAYERS 2
#define DIMENSION 3
#define LAMINATIONSCALE 0.1
#define EPS 0.000001

#define CRACK_FRONT 2
#define CRACK_FACE 1
#define OUTER_CRACK 0

#define CRACK_SURFACE 1
#define OTHER 0

int nnodes;
int completed_nnodes;
int number_of_inner_nodes;
int number_of_innerest_nodes;
int npatch;
int minus_flag[NUMBER_OF_CRACKFRONT_POINTS];
int inout_flag[NUMBER_OF_CRACKFRONT_POINTS];
int crack_flag[NUMBER_OF_CRACK_POINTS];
int surface_flag[NUMBER_OF_CRACK_POINTS];
int new_nodes_flag[NUMBER_OF_LAMINATION_POINTS];
double nodes_coordinate[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double completed_nodes_coordinate[NUMBER_OF_CRACK_POINTS][DIMENSION];
double completed_normal_vector[NUMBER_OF_CRACK_POINTS][DIMENSION];
double inner_nodes_coordinate[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double innerest_nodes_coordinate[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double patch_coord[NUMBER_OF_PATCHS][NUMBER_OF_TRIANGLE_VERTEXS][DIMENSION];
double patch_cg[NUMBER_OF_PATCHS][DIMENSION];
double init_univec_normal[DIMENSION];
double univec_normal[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double univec_propa[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double univec_tangent[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double innerest_univec_normal[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double innerest_univec_propa[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double innerest_univec_tangent[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double p_to_p_vector[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double innerest_nodes_p_to_p_vector[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
double layer_size;
double scale_factor;
double lamination_weight[NUMBER_OF_CRACKFRONT_POINTS];
double layer_length[NUMBER_OF_CRACKFRONT_POINTS];
double layer_length_sum;
char patch_file_name[256];
double temp_crackface_new_nodes_coordinate[NUMBER_OF_CRACK_POINTS][DIMENSION];
double temp_crackface_new_normal_vector[NUMBER_OF_CRACK_POINTS][DIMENSION];
int temp_count;

/*
 * 入力された２点の距離を求めて出力する関数
 */
double Distance(double a[], double b[])
{
  double dist;
  dist = sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));
  return dist;
}


/*
 * 入力された２つのベクトルの内積を求めて出力する関数
 */
double InnerProduct(double a[], double b[])
{
  double product;
  product = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return product;
}

/*
 * ベクトルa,bの外積ベクトルcを算出
 * (a×b=c)
 */
void CrossProduct(double a[], double b[], double c[])
{
  c[0] = a[1]*b[2] - a[2]*b[1];
  c[1] = a[2]*b[0] - a[0]*b[2];
  c[2] = a[0]*b[1] - a[1]*b[0];
}

/*
 * 入力したベクトルの長さを出力する関数
 */
double GetVectorLength(double a[])
{
  double length;
  length = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  return length;
}

/*
 * データが入った配列のstartからtotalまでのデータの値を比較し、
 * 最大のものと最小のものの"配列番号"を出力する関数
 */
void GetMaxandMin(int start, int total, double value[], int *max, int *min)
{
  int i;
  double max_value;
  double min_value;
  max_value = value[start];
  *max = start;
  min_value = value[start];
  *min = start;
  for(i = start; i < total; i++){
    if(max_value < value[i]){
      max_value = value[i];
      *max = i;
    }
    if(min_value > value[i]){
      min_value = value[i];
      *min = i;
    }
  }
}

/*
 * 入力した３＊３の行列を逆行列にして出力する関数
 */
int InverseMatrix_3D( double M[3][3], double zero ){
  int i, j;
  double a[3][3];
  double det = M[0][0]*M[1][1]*M[2][2] +M[0][1]*M[1][2]*M[2][0] +M[0][2]*M[1][0]*M[2][1]
    -M[0][0]*M[1][2]*M[2][1] -M[0][2]*M[1][1]*M[2][0] -M[0][1]*M[1][0]*M[2][2];

  if(fabs(det) < zero) return(1); // matrix is singular 

  for( i = 0; i < 3; i++ ){
    for( j = 0; j < 3; j++ )	a[i][j] = M[i][j];
  }
  M[0][0] = (a[1][1]*a[2][2]-a[1][2]*a[2][1])/det; M[0][1] = (a[0][2]*a[2][1]-a[0][1]*a[2][2])/det; M[0][2] = (a[0][1]*a[1][2]-a[0][2]*a[1][1])/det;
  M[1][0] = (a[1][2]*a[2][0]-a[1][0]*a[2][2])/det; M[1][1] = (a[0][0]*a[2][2]-a[0][2]*a[2][0])/det; M[1][2] = (a[0][2]*a[1][0]-a[0][0]*a[1][2])/det;
  M[2][0] = (a[1][0]*a[2][1]-a[1][1]*a[2][0])/det; M[2][1] = (a[0][1]*a[2][0]-a[0][0]*a[2][1])/det; M[2][2] = (a[0][0]*a[1][1]-a[0][1]*a[1][0])/det;
  return (0);
}

/*
 * 入力した２つのベクトルの平均を出力する関数
 */
void GetAverageVector(double a[], double b[], double c[])
{
  int i;
  for(i = 0; i < DIMENSION; i++){
    c[i] = (a[i] + b[i])/2;
  }
}

/*
 * 入力したベクトルを単位ベクトルへと変換する関数
 */
void GetUnitVector(double a[])
{
  double temp_amount;
  temp_amount = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);

  a[0] = a[0]/temp_amount;
  a[1] = a[1]/temp_amount;
  a[2] = a[2]/temp_amount;
}

void ReadNodes(const char *filename)
{
  int i,nodeid[NUMBER_OF_CRACKFRONT_POINTS];
  double dummy;
  FILE *fp;

  fp = fopen (filename,"r");
  assert(fp!=NULL);

  fscanf(fp,"%d \n",&nnodes);

  for(i=0;i<nnodes;i++)
  {
    fscanf(fp,"%d %le %le %le\n",&nodeid[i],&nodes_coordinate[i][0],&nodes_coordinate[i][1],&nodes_coordinate[i][2]);
  }
}

/*
 * 半円で定義したき裂の中心
 * 横方向ベクトル、奥行きベクトル、その二つの垂直ベクトルを
 * 読み取る関数
 * このプログラムでは垂直ベクトルを逆向きにしたものしか使用しない
 */
void ReadMapCrackNode(const char *filename)
{
  FILE *fp;
  double Origin[3];
  double USize[3];
  double VSize[3];
  double WDir[3];

  fp = fopen (filename,"r");
  assert(fp!=NULL);

  fscanf (fp, "%lf %lf %lf", 
	  &Origin[0], &Origin[1], &Origin[2]);
  fscanf (fp, "%lf %lf %lf", 
	  &USize[0], &USize[1], &USize[2]);
  fscanf (fp, "%lf %lf %lf", 
	  &VSize[0], &VSize[1], &VSize[2]);
  fscanf (fp, "%lf %lf %lf", 
	  &WDir[0], &WDir[1], &WDir[2]);

    init_univec_normal[0] = -WDir[0];
    init_univec_normal[1] = -WDir[1];
    init_univec_normal[2] = -WDir[2];
    printf("init_univec_normal = %lf %lf %lf\n", init_univec_normal[0], init_univec_normal[1], init_univec_normal[2]);
  fclose(fp);
}

void ReadPatch(const char *fileName)
{
  //static int npatch;
  //static double patch_coord[MAX_PATCH][3][3];

  FILE *fp;
  int ipatch, ip, idir;
  int idummy;

  fp = fopen (fileName, "r");
  assert(fp!=NULL);

  fscanf(fp,"%d", &npatch);

  for(ipatch=0; ipatch < npatch; ipatch++)
  {
    fscanf(fp,"%d",&idummy);
    for(ip=0; ip < 3; ip++)
      for(idir=0; idir < 3; idir++)
        fscanf(fp,"%lf",&patch_coord[ipatch][ip][idir]);
  }
  fclose(fp);
}

#define INNEREST "___temp_innerest.node"
#define INNERESTINOUT "___temp_innerest.node_inout"

void WriteNodes(int total, double nodes_coordinate[][DIMENSION])
{
  const char filename[200] = "___temp_innerest.node";
  FILE *fp;
  int inode,idir;

  fp = fopen(filename, "w");
  assert(fp!=NULL);

  fprintf(fp, "%d\n", total);

  for(inode = 0; inode < total; inode++){
    fprintf(fp, "%d %lf %lf %lf\n", inode, nodes_coordinate[inode][0],nodes_coordinate[inode][1],nodes_coordinate[inode][2]);
  }
  fclose(fp);
}

void ReadNodesInOut(int total)
{

  const char *filename = INNERESTINOUT;
  FILE *fp;
  int temp_total;
  int i;
  int dummy;
  fp = fopen(filename, "r");

  assert(fp!=NULL);

  fscanf(fp, "%d", &temp_total);
  assert(total == temp_total);

  
  for(i = 0; i < temp_total; i++){
    fscanf(fp, "%d %d", &dummy, &inout_flag[i]);
  }
}

void WriteCompleteNodes(const char *filename)
{
  FILE *fp;
  int inode,idir;

  fp = fopen(filename, "w");

  fprintf(fp, "%d\n", completed_nnodes);

  for(inode = 0; inode < completed_nnodes; inode++){
    fprintf(fp, "%d %lf %lf %lf\n", inode, completed_nodes_coordinate[inode][0],completed_nodes_coordinate[inode][1],completed_nodes_coordinate[inode][2]);
  }
  fclose(fp);
}

void WriteCrackFlag(const char *filename)
{
  FILE *fp;
  int inode;

  fp = fopen(filename, "w");

  fprintf(fp, "%d\n", completed_nnodes);

  for(inode = 0; inode < completed_nnodes; inode++){
    fprintf(fp, "%d %d\n", inode, crack_flag[inode]);
  }
  fclose(fp);
}

void WriteCrackFrontFlag(const char *filename)
{
  FILE *fp;
  int inode;

  fp = fopen(filename, "w");

  fprintf(fp, "%d\n", completed_nnodes);

  for(inode = 0; inode < completed_nnodes; inode++){
    if(crack_flag[inode] == CRACK_FRONT){
    fprintf(fp, "%d %d\n", inode, 1);
    }
    if(crack_flag[inode] == CRACK_FACE || crack_flag[inode] == OTHER){
      fprintf(fp, "%d %d\n", inode, 0);
    }
  }
  fclose(fp);
}

void WriteSurfaceFlag(const char *filename)
{
  FILE *fp;
  int inode;

  fp = fopen(filename, "w");

  fprintf(fp, "%d\n", completed_nnodes);

  for(inode = 0; inode < completed_nnodes; inode++){
    fprintf(fp, "%d %d\n", inode, surface_flag[inode]);
  }
  fclose(fp);
}

void WriteNormal(const char *filename)
{
  FILE *fp;
  int inode;

  fp  = fopen(filename, "w");

  fprintf(fp, "%d\n", completed_nnodes);

  for(inode = 0; inode < completed_nnodes; inode++){
    fprintf(fp , "%d %lf %lf %lf\n", inode, completed_normal_vector[inode][0], completed_normal_vector[inode][1], completed_normal_vector[inode][2]);
  }

  fclose(fp);
}

/*
 * 最終的に出力する点を登録する関数
 */
void AddNewNode(double new_nodes_coordinate[], double new_normal_vector[], int temp_crack_flag, int temp_surface_flag)
{
  int i;
  for(i = 0; i < DIMENSION; i++){
    completed_nodes_coordinate[completed_nnodes][i] = new_nodes_coordinate[i];
    completed_normal_vector[completed_nnodes][i] = new_normal_vector[i];
  }
  crack_flag[completed_nnodes] = temp_crack_flag;
  surface_flag[completed_nnodes] = temp_surface_flag;
  completed_nnodes++;
  assert(completed_nnodes < NUMBER_OF_CRACK_POINTS);
}

/*
 * 最終的に出力する前に仮点として一時登録する関数
 */
void TempAddNewNode(double new_nodes_coordinate[], double new_normal_vector[])
{
  int i;
  for(i = 0; i < DIMENSION; i++){
  temp_crackface_new_nodes_coordinate[temp_count][i] = new_nodes_coordinate[i];
  temp_crackface_new_normal_vector[temp_count][i] = new_normal_vector[i];
  }
  new_nodes_flag[temp_count] = 1;
  temp_count++;
  assert(temp_count < NUMBER_OF_CRACK_POINTS);
}

void ClearNumberOfInnerestNodes()
{
  number_of_innerest_nodes = 0;
}

void ClearInOutFlag()
{
  int i;
  for(i = 0; i < NUMBER_OF_CRACKFRONT_POINTS; i++){
    inout_flag[i] = 1;
  }
}

void ClearNewNodeFlag()
{
  int i;
  for (i = 0; i <NUMBER_OF_LAMINATION_POINTS; i++)
    new_nodes_flag[i] = 0;
}

/*
 * 前縁から前後に２層ずつ積層させたあと、その中で最も内側の層の点を
 * Laminationの基準にするために登録する関数
 */
void AddInnerNode(double new_nodes_coordinate[])
{
  int i;
  for(i = 0; i < DIMENSION; i++){
    inner_nodes_coordinate[number_of_inner_nodes][i] = new_nodes_coordinate[i];
  }
  number_of_inner_nodes++;
}

/*
 * き裂前縁を.nodeとして登録
 */
void ResisterCrackFrontNodes(int total, double temp_nodes_coordinate[][DIMENSION], double temp_univec_normal[][DIMENSION])
{
  int i;
  for(i = 0; i < total; i++){
    if(i == 0 || i == total-1){
      AddNewNode(temp_nodes_coordinate[i], temp_univec_normal[i], CRACK_FRONT, CRACK_SURFACE);
    } else {
      AddNewNode(temp_nodes_coordinate[i], temp_univec_normal[i], CRACK_FRONT, OTHER);
    }
  }
}

//patch_cgとして表面パッチの重心を登録
void CompPatchCG(void)
{
  int iPatch, inode, idir;

  for(iPatch=0; iPatch < npatch; iPatch++)
    for(idir=0; idir<3; idir++)
      patch_cg[iPatch][idir] 
        = (patch_coord[iPatch][0][idir]
            + patch_coord[iPatch][1][idir] + patch_coord[iPatch][2][idir])/3.0;

}

//ThreshValとしてすべてのパッチで最も長い辺の＊５をreturn
double CompThreshDist(void)
{
  // find the smappset patch;

  double min = 1000000000000.0;
  double max = 0.0;
  int iPatch;
  int idir,i0, i1;
  double TempVec[3], Abs_value;
  double ThreshVal;

  for(iPatch=0; iPatch < npatch; iPatch++)
  {
    for(i0=0; i0<3; i0++)
    {
      if(i0<2) i1=i0+1;
      if(i0==2) i1 = 0;
      for(idir=0; idir<3; idir++)
        TempVec[idir] 
          = patch_coord[iPatch][i1][idir] - patch_coord[iPatch][i0][idir];

      Abs_value = sqrt(TempVec[0]*TempVec[0] + TempVec[1]*TempVec[1]
          + TempVec[2]*TempVec[2]);
      if(Abs_value < min) min = Abs_value;
      if(Abs_value > max) max = Abs_value;	  
    }
  }

  ThreshVal = max*5;
  return(ThreshVal);
}

//ある点から出ているベクトルと三角形パッチが交わるときの
//vecA,vecB,-vecTの係数を引数として出力
int CompCrossingPt_TriangleAndLine(double vecA[3], double vecB[3], 
    double vecT[3], double xxT[3], double xx0[3],
    double *pp, double *qq, double *mm, double zero)
{
  double matA[3][3], rhsv[3], solvec[3];
  int ii, jj;

  for(ii=0; ii<3; ii++){
    matA[ii][0] = vecA[ii];
    matA[ii][1] = vecB[ii];
    matA[ii][2] = -vecT[ii];

    rhsv[ii] = xx0[ii] - xxT[ii];
  }

  if(InverseMatrix_3D(matA, zero) == 1) return(1); //matrix is singular

  for(ii=0; ii<3; ii++) solvec[ii]=0.0;
  for(ii=0; ii<3; ii++)
    for(jj=0; jj<3; jj++)
      solvec[ii] += matA[ii][jj] * rhsv[jj];

  *pp = solvec[0];
  *qq = solvec[1];
  *mm = solvec[2];

  return(0);
}

/*
 * １層の長さを測る関数
 */
void GetLayerLength(int total, double nodes_coordinate[][DIMENSION])
{
  int i, j;
  double value;
  double sum;
  double temp_vector[DIMENSION];
  for(i = 0; i < total - 1; i++){
    for(j = 0; j < DIMENSION; j++){
      temp_vector[j] = nodes_coordinate[i+1][j] - nodes_coordinate[i][j];
    }
    layer_length[i] = sqrt(temp_vector[0]*temp_vector[0] + temp_vector[1] * temp_vector[1] + temp_vector[2] * temp_vector[2]);
    layer_length_sum += layer_length[i];
  }
}

/*
 * 積層した際に点群の間隔が詰まっていたり広まっていたりしたら
 * また一定の距離になるように最配置する関数
 */
void RearrangeLayer(int *total, double nodes_coordinate[][DIMENSION])
{
  int i, j;
  int number_of_division;
  int count = 0;
  int nodes_count = 0;
  double temp_nodal_distance;
  double vector_coefficient = 0.0;
  double temp_nodes_coordinate[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];
  double distance;

  for(i = 0; i < *total; i++){
  }
  number_of_division = layer_length_sum/layer_size + 0.5;
  temp_nodal_distance = layer_length_sum/number_of_division;
  for(j = 0; j < DIMENSION; j++){
    temp_nodes_coordinate[0][j] = nodes_coordinate[0][j];
  }
  count++;

  vector_coefficient = temp_nodal_distance;
  while(1){
    if(vector_coefficient < layer_length[nodes_count]){
      for(j = 0; j < DIMENSION; j++){
        temp_nodes_coordinate[count][j] = nodes_coordinate[nodes_count][j] + vector_coefficient/layer_length[nodes_count] * (nodes_coordinate[nodes_count+1][j] - nodes_coordinate[nodes_count][j]);
      }
      distance =  Distance(temp_nodes_coordinate[count], temp_nodes_coordinate[count-1]);
      vector_coefficient = vector_coefficient + temp_nodal_distance;
      count++;
    } else {
      if((*total) - 1 == nodes_count) break;
      vector_coefficient = vector_coefficient - layer_length[nodes_count];
      nodes_count++;
    }
  }
 count--; 
  for(j = 0; j < DIMENSION; j++){
    temp_nodes_coordinate[count][j] = nodes_coordinate[(*total)-1][j];
  }
    count++;

  for(i = 0; i < count; i++){
    for(j = 0; j < DIMENSION; j++){
      nodes_coordinate[i][j] = temp_nodes_coordinate[i][j];
    }
  }
  *total = count;
}

//き裂前縁メッシュサイズ及び層間距離読み込み
void ReadCrackParam(const char *fileName)
{
  FILE *fp;
  double d1,d2,d3;

  fp = fopen (fileName, "r");

  if (fp == NULL) {
    fprintf (stderr, " file %s not found \n", fileName);
    exit(1);
  }

  fscanf(fp,"%lf %lf",&d1,&d2);
  fscanf(fp,"%lf",&d3);

  layer_size = d3;
}

/*
 * き裂前縁点の座標値を用いて始点から終点に向かう方向に１点ずつ点を結ぶベクトルを作成
 */
void GetPointtoPointVector(int temp_nnodes, double temp_nodes_coordinate[][DIMENSION], double temp_p_to_p_vector[][DIMENSION])
{
  int i, j;
  for(i = 0; i < temp_nnodes-1; i++){
    for(j = 0; j < DIMENSION; j++){
      temp_p_to_p_vector[i][j] = temp_nodes_coordinate[i+1][j] - temp_nodes_coordinate[i][j];
    }
      GetUnitVector(temp_p_to_p_vector[i]);
  }
}

/*
 * 最初の積層をさせる時に最も内側（外側）の層までの距離を規定
 */
void SetScaleFactor()
{
  scale_factor = layer_size * 2;
}

void ClearMinusFlag()
{ int i;
  for(i = 0; i < NUMBER_OF_CRACKFRONT_POINTS; i++){
    minus_flag[i] = 0;
  }
}

/*
 * 各前縁点における接線方向ベクトル、法線方向ベクトル、進展方向ベクトルを作成
 * ただし法線方向ベクトルはReadParamで読み取った値を使用
 */
void GetUnivectoratAllPoint(int temp_nnodes, double temp_p_to_p_vector[][DIMENSION], double temp_univec_normal[][DIMENSION], double temp_univec_propa[][DIMENSION], double temp_univec_tangent[][DIMENSION])
{
  int i, j;
  double product;
  for(j = 0; j < DIMENSION; j++){
    temp_univec_tangent[0][j] = temp_p_to_p_vector[0][j];
    temp_univec_normal[0][j] = init_univec_normal[j];
  }
  CrossProduct(temp_univec_normal[0], temp_univec_tangent[0], temp_univec_propa[0]);
  GetUnitVector(temp_univec_propa[0]);


  for(i = 1; i < temp_nnodes - 1; i++){
  for(j = 0; j < DIMENSION; j++){
    temp_univec_normal[i][j] = init_univec_normal[j];
  }
    GetAverageVector(temp_p_to_p_vector[i-1], temp_p_to_p_vector[i], temp_univec_tangent[i]);
    CrossProduct(temp_univec_normal[i], temp_univec_tangent[i], temp_univec_propa[i]);
    GetUnitVector(temp_univec_propa[i]);
  }
  for(j = 0; j < DIMENSION; j++){
    temp_univec_tangent[temp_nnodes-1][j] = temp_p_to_p_vector[temp_nnodes-2][j];
    temp_univec_normal[temp_nnodes-1][j] = init_univec_normal[j];
  }
  CrossProduct(temp_univec_normal[temp_nnodes-1], temp_univec_tangent[temp_nnodes-1], temp_univec_propa[temp_nnodes-1]);
  GetUnitVector(temp_univec_propa[temp_nnodes-1]);
}

/*
 * AdvVecのNormal方向と垂直な成分を算出する関数
 */
void SurfaceCorrectionAdvVec(double AdvVec[3], double Normal[3])
{
  double InnerProd;

  InnerProd = AdvVec[0]*Normal[0]+AdvVec[1]*Normal[1]+AdvVec[2]*Normal[2];
  AdvVec[0] = AdvVec[0] - InnerProd * Normal[0];
  AdvVec[1] = AdvVec[1] - InnerProd * Normal[1];
  AdvVec[2] = AdvVec[2] - InnerProd * Normal[2];

}

/*
 * き裂の端からの接線方向ベクトルと三角形面内で交わる
 * 位置を求め、接線方向ベクトルの係数と、三角形の辺のベクトルの
 * 係数を算出する関数
 */
void ComputeNormalNormalAndMovingVec(double CoordVec[3], double TanVec[3],
    double Normal[3], double MovVec[3], 
    double DistVec)
{

  int idir, iPatch;
  double pt_coord[3], pt_CG[3], dist1;
  double vecA[3], vecB[3], vecT[3], xx0[3], xxT[3];
  double pp,qq,mm;
  double zero = 0.000000000000001;
  double epsilon = 0.0000001;
  int if_singular;
  double Min_mm = 100000000000.0;
  double pp_keep, qq_keep, mm_keep;
  int iPatch_keep, iCount = 0;
  double ThreshDist;

  //patch_cgとして表面パッチの重心を登録
  CompPatchCG();
  //すべてのパッチで最も長い辺の＊５をreturnする関数
  ThreshDist = CompThreshDist();

  for(idir=0; idir<3; idir++){
    pt_coord[idir] = CoordVec[idir];
    xx0[idir] = CoordVec[idir]; 
    vecT[idir] = TanVec[idir];
  }

  for(iPatch=0; iPatch < npatch; iPatch++){
    pt_CG[0] = patch_cg[iPatch][0];
    pt_CG[1] = patch_cg[iPatch][1];
    pt_CG[2] = patch_cg[iPatch][2];

    //パッチの重心から判定したい点までの距離を算出
    dist1 = sqrt((pt_CG[0]-pt_coord[0])*(pt_CG[0]-pt_coord[0])
        +(pt_CG[1]-pt_coord[1])*(pt_CG[1]-pt_coord[1])
        +(pt_CG[2]-pt_coord[2])*(pt_CG[2]-pt_coord[2]));

    //もしThreshDistより小さければ
    if(dist1 < ThreshDist){
      for(idir=0; idir<3; idir++){
        vecA[idir] = patch_coord[iPatch][1][idir] 
          - patch_coord[iPatch][0][idir];
        vecB[idir] = patch_coord[iPatch][2][idir] 
          - patch_coord[iPatch][0][idir];
        xxT[idir] = patch_coord[iPatch][0][idir];
      }

      //き裂前縁の端の点から出ているベクトルと三角形パッチが交わるときの
      //vecA,vecB,-vecTの係数を引数として出力
      if_singular 
        = CompCrossingPt_TriangleAndLine(vecA, vecB, vecT, xxT, xx0,
            &pp, &qq, &mm, zero);

      if(if_singular == 0){
        ////き裂前縁の端の点から出ているベクトルと三角形パッチが交わるときの
        //vecA,vecB,-vecTの係数の係数をみて、三角形が成す平面と
        //ベクトルの交点がパッチの面内にあり、点からパッチまでの距離が最も近いところをiPatch_keepとする
        if(Min_mm > fabs(mm) && (pp+qq) < (1.0+epsilon) && 
            pp > (-epsilon) && qq > (-epsilon)){
          Min_mm = fabs(mm);
          pp_keep = pp;
          qq_keep = qq;
          mm_keep = mm;
          iPatch_keep = iPatch;
          iCount++;
        }
      }
    }
  }


  if(iCount == 0){
    printf("Error counter is ZERO: Serching in MovingCoordinate\n");
    exit(1);
  }   
  for(idir=0; idir<3; idir++){
    vecA[idir] = patch_coord[iPatch_keep][1][idir] 
      - patch_coord[iPatch_keep][0][idir];
    vecB[idir] = patch_coord[iPatch_keep][2][idir] 
      - patch_coord[iPatch_keep][0][idir];
    MovVec[idir] = mm_keep * vecT[idir];
  }
  //パッチが成す面の法線ベクトル（平面の方程式の導出に使用）を算出
  CrossProduct(vecA, vecB, Normal);
  //法線ベクトルを単位ベクトルに
  GetUnitVector(Normal);
  //DistVecに点からパッチまでの距離を登録
  DistVec = sqrt(MovVec[0]*MovVec[0]+MovVec[1]*MovVec[1]+MovVec[2]*MovVec[2]);

}

/*
 * 始点と終点における進展方向ベクトルを表面に補正する関数
 */
void VectorCorrectiontoSurface()
{

  int i,j;
  double CoordVecStart[3], CoordVecEnd[3]; 
  double AdvCoordVecStart1[3], AdvCoordVecEnd1[3];
  double AdvCoordVecStart2[3], AdvCoordVecEnd2[3];
  double TanVecStart[3], TanVecEnd[3];
  double AdvVecStart[3], AdvVecEnd[3];
  double NormalStart[3], NormalEnd[3];
  double MovVecStart[3], MovVecEnd[3];
  double AdvVecStart1[3], AdvVecEnd1[3];
  double AdvVecStart2[3], AdvVecEnd2[3];
  double weight1=0.5, weight2=0.5;
  double DistVecStart, DistVecEnd;

  //き裂前縁端の点２点をStartとEndとして登録
  //それぞれ、隣の点から端の点へと向かうベクトルをTanVecとして登録
  //進展ベクトルにscale_factorを掛けたものをAdvVecとして登録
  for(i=0; i<3; i++){
    CoordVecStart[i] = nodes_coordinate[0][i];
    CoordVecEnd[i] =  nodes_coordinate[nnodes-1][i];
    TanVecStart[i] = -univec_tangent[0][i];
    TanVecEnd[i] = univec_tangent[nnodes-1][i];
    AdvVecStart[i] = univec_propa[0][i]*scale_factor;
    AdvVecEnd[i] = univec_propa[nnodes-1][i]*scale_factor;
  }  

  //TanVecを単位ベクトルに変換
  GetUnitVector(TanVecStart);
  GetUnitVector(TanVecEnd);

  //き裂の端の点について、Tanvecの方向に存在する最も近いパッチが成す
  //法線ベクトルと、端の点からパッチを結ぶベクトルと、その長さを導出
  //(ただし前ステップのき裂前縁点はそもそも表面に存在するので
  //MovVecとDistVecはほぼ無意味)
  ComputeNormalNormalAndMovingVec(CoordVecStart,TanVecStart,NormalStart,
      MovVecStart, DistVecStart);
  ComputeNormalNormalAndMovingVec(CoordVecEnd,TanVecEnd,NormalEnd,
      MovVecEnd, DistVecEnd);

  //進展ベクトルを法線ベクトルに対して垂直になるように補正
  //(進展ベクトルの法線に垂直な方向の成分を取り出す感じ)
  SurfaceCorrectionAdvVec(AdvVecStart, NormalStart);
  SurfaceCorrectionAdvVec(AdvVecEnd, NormalEnd);

  for(i=0;i<3;i++){
    //き裂前縁の端の点をAdvVecだけ動かした点
    AdvCoordVecStart1[i] = CoordVecStart[i] + AdvVecStart[i];
    AdvCoordVecEnd1[i] = CoordVecEnd[i] + AdvVecEnd[i];
    TanVecStart[i] = NormalStart[i];
    TanVecEnd[i] = NormalEnd[i];
  }

  //き裂の端の点からAdvVecだけ進んだ点について、Tanvecの方向に存在する最も近いパッチが成す
  //法線ベクトルと、端の点からパッチを結ぶベクトルと、その長さを導出
  ComputeNormalNormalAndMovingVec(AdvCoordVecStart1,TanVecStart,NormalStart,
      MovVecStart, DistVecStart);
  ComputeNormalNormalAndMovingVec(AdvCoordVecEnd1,TanVecEnd,NormalEnd,
      MovVecEnd, DistVecEnd);

  //き裂の端の点からAdvVecだけ進んだ点を最も近いパッチ面上へ移動
  for(i=0; i<3; i++){
    AdvCoordVecStart2[i] = AdvCoordVecStart1[i] + MovVecStart[i];
    AdvCoordVecEnd2[i] =  AdvCoordVecEnd1[i] + MovVecEnd[i];
  } 

  //き裂端の点から垂直補正した後のAdvVecだけ進んだ点までのベクトルと
  //き裂端の点から垂直補正した後のAdvVecだけ進んだ点からパッチ上へ移動させた点へのベクトルを登録
  for(i=0; i<DIMENSION; i++){
    AdvVecStart1[i] = AdvCoordVecStart1[i] - CoordVecStart[i];
    AdvVecEnd1[i] = AdvCoordVecEnd1[i] - CoordVecEnd[i];
    AdvVecStart2[i] = AdvCoordVecStart2[i] - CoordVecStart[i];
    AdvVecEnd2[i] = AdvCoordVecEnd2[i] - CoordVecEnd[i];
  }

  //重みを半々として端の点における進展ベクトルを登録
  for(i=0; i<DIMENSION; i++){
    univec_propa[0][i]= (AdvVecStart1[i]*weight1+AdvVecStart2[i]*weight2)
      /scale_factor;
    univec_propa[nnodes-1][i] =(AdvVecEnd1[i]*weight1+AdvVecEnd2[i]*weight2)
      /scale_factor;
  }
}

/*
 * き裂前縁からそれぞれ内側と外側に向けて２層ずつ積層させる関数
 */
void MakeInOutLayer()
{
  int i,j,k;
  double coord_of_outer_layer[DIMENSION];
  double coord_of_inner_layer[DIMENSION];
  for(i = 0; i < NUMBER_OF_OUTER_LAYERS; i++){
    for(j = 0; j < nnodes; j++){
      for(k = 0; k < DIMENSION; k++){
        coord_of_outer_layer[k] = nodes_coordinate[j][k] + layer_size * (i+1) * univec_propa[j][k];
      }
      if(j == 0 || j == nnodes-1){
        AddNewNode(coord_of_outer_layer, univec_normal[j], OUTER_CRACK, CRACK_SURFACE);
      } else {
        AddNewNode(coord_of_outer_layer, univec_normal[j], OUTER_CRACK, OTHER);
      }
    }
  }

  ClearNumberOfInnerestNodes();

  for(i = 0; i < NUMBER_OF_INNER_LAYERS; i++){
    for(j = 0; j < nnodes; j++){
      for(k = 0; k < DIMENSION; k++){
        coord_of_inner_layer[k] = nodes_coordinate[j][k] - layer_size * (i+1) * univec_propa[j][k];        
      }
      if(j == 0 || j == nnodes-1){
        AddNewNode(coord_of_inner_layer, univec_normal[j], CRACK_FACE, CRACK_SURFACE);
      } else {
        AddNewNode(coord_of_inner_layer, univec_normal[j], CRACK_FACE, OTHER);
      }
      if(i == NUMBER_OF_INNER_LAYERS-1){
        AddInnerNode(coord_of_inner_layer);
      }
    }
  }
  char *filename = "debug_test.node";
  WriteCompleteNodes(filename);
  char *filename_cf = "debug_test.nd_crack";
  WriteCrackFlag(filename_cf);
  char *filename_surf = "debug_test.nd_surf";
  WriteSurfaceFlag(filename_surf);
}

/*
 * 内外判定のプログラムを回す関数
 */
void DecisionOfInOut(double temp_nnodes, double temp_nodes_coordinate[][DIMENSION])
{
  char buf[256];
  WriteNodes(temp_nnodes, temp_nodes_coordinate);
  sprintf(buf, "~/ADVENTURE/advauto_etc_new/autoage/hetare/gm3d/command/advautoage_h_gm3d_tri_inout2 %s %s %s", patch_file_name, INNEREST, INNERESTINOUT);
  system(buf);
  fflush(stdout);
  ReadNodesInOut(temp_nnodes);
}

/*
 * 積層の際に凸部の積層距離を長く、
 * 凹部の積層距離を短くするために
 * 外積と内積からなる重みを使用している
 * temp_p_to_p_vectorの外積の絶対値を重みとし、
 * ２つの傾きが９０度を超える（内積の値が負になるなら）
 * ２から外積の値を引く、この操作をした後に
 * もし外積が負なら負の値をかける
 * 重みは0から1の間に抑えたいのでその場所での最小値を
 * 最大値から最小値を引いたもので割っている
 * さらに最小の部分でも少しは進展するように+0.1
 * してから1.1で割っている
 */
void CalculateLaminationWeight(int temp_nnodes, double temp_p_to_p_vector[][DIMENSION], double temp_lamination_weight[])
{
  int i;
  int max, min;
  double minus_product;
  double temp_inner_product;
  double temp_cross_product[DIMENSION];
  double temp_cross_product_length;
  double temp_value[NUMBER_OF_CRACKFRONT_POINTS];
  double temp_temp_value[NUMBER_OF_CRACKFRONT_POINTS];
  for(i = 1; i < temp_nnodes - 1; i++){
    CrossProduct(temp_p_to_p_vector[i], temp_p_to_p_vector[i-1], temp_cross_product);
    minus_product = InnerProduct(temp_cross_product, init_univec_normal);
    if(minus_product < 0) minus_flag[i] = 1;
    temp_inner_product = InnerProduct(temp_p_to_p_vector[i], temp_p_to_p_vector[i-1]);
    temp_cross_product_length = GetVectorLength(temp_cross_product);
    if(temp_inner_product >= 0){
      temp_value[i] = temp_cross_product_length;
    } else {
      temp_value[i] = 2 - temp_cross_product_length;
    }
    if(minus_flag[i] == 1){
      temp_value[i] = -temp_value[i];
    }
  }
  GetMaxandMin(1, temp_nnodes-1, temp_value, &max, &min);
  for(i = 1; i < temp_nnodes - 1; i++){
    temp_temp_value[i] = temp_value[i] - temp_value[min];
    temp_lamination_weight[i] = temp_temp_value[i]/(temp_value[max] - temp_value[min]);
  }
}


/*
 * 積層させたうちの最も内側の層の各univec（進展方向、接線方向、法線方向ベクトルを総称してunivecとする）を算出、
 * 進展方向のベクトルに負をかけ、形状に応じて重み付けした係数をかけて、積層の間隔の1/10だけ内側に層を作成
 * もし点群が前縁の節点間距離とはかけ離れた間隔になった場合は再配置
 * パッチを用いて内外判定、外側に出た点については次の積層時に移動しないものとする
 * 以上を繰り返して層を作成した時に全ての点がパッチの外側に出たら終了
 *
 * 一番内側の層を更新していくことで積層を表現しているが、最後の全ての点がパッチの外に出た時の層をき裂表面点として扱う
 * また、積層していく段階で更新された点も次のステップのために一時的に保管する
 */
void LaminationLayer()
{
  int i,j;
  int inside_count;
  int rearrange_count = 0;
  double lamination_scale_factor = layer_size * LAMINATIONSCALE;
  double temp_innerest_nodes_coordinate[NUMBER_OF_CRACKFRONT_POINTS][DIMENSION];

  number_of_innerest_nodes = number_of_inner_nodes;
  for(i = 0; i < number_of_inner_nodes; i++){
    for(j = 0; j < DIMENSION;j++){
      innerest_nodes_coordinate[i][j] = inner_nodes_coordinate[i][j];
    }
  }    
  
  GetPointtoPointVector(number_of_innerest_nodes, innerest_nodes_coordinate, innerest_nodes_p_to_p_vector);
    GetUnivectoratAllPoint(number_of_innerest_nodes, innerest_nodes_p_to_p_vector, innerest_univec_normal, innerest_univec_propa, innerest_univec_tangent);
    CalculateLaminationWeight(number_of_innerest_nodes, innerest_nodes_p_to_p_vector, lamination_weight);


  temp_count = 0;
  ClearNewNodeFlag();
  ClearInOutFlag();
  int count;
  //for(count = 0; count < 1000; count++){
    //printf("count = %d\n", count);
  while(1){
    inside_count = 0;
    ClearMinusFlag();
    for(i = 1; i < number_of_innerest_nodes - 1; i++){
      if(inout_flag[i] == 1){
        for(j = 0; j < DIMENSION; j++){
          temp_innerest_nodes_coordinate[i][j] = innerest_nodes_coordinate[i][j] - lamination_scale_factor * lamination_weight[i] * innerest_univec_propa[i][j];
          innerest_nodes_coordinate[i][j] = temp_innerest_nodes_coordinate[i][j];
        }
        inside_count++;
      }
    }
    layer_length_sum = 0.0;
    GetLayerLength(number_of_innerest_nodes, innerest_nodes_coordinate);

    ClearInOutFlag();
    for(i = 0; i < number_of_innerest_nodes - 1; i++){
      if(layer_size * 0.8 > layer_length[i] || layer_size * 1.2 < layer_length[i]){
        RearrangeLayer(&number_of_innerest_nodes, innerest_nodes_coordinate);
        printf("Rearranged Layer\n");
        break;
      }
    }    
    
    GetPointtoPointVector(number_of_innerest_nodes, innerest_nodes_coordinate, innerest_nodes_p_to_p_vector);
    GetUnivectoratAllPoint(number_of_innerest_nodes, innerest_nodes_p_to_p_vector, innerest_univec_normal, innerest_univec_propa, innerest_univec_tangent);

    for(i = 1; i < number_of_innerest_nodes - 1; i++){
      for(j = 0; j < DIMENSION; j++){
      }
    }
    CalculateLaminationWeight(number_of_innerest_nodes, innerest_nodes_p_to_p_vector, lamination_weight);
    for(i = 1; i < number_of_innerest_nodes - 1; i++){
      TempAddNewNode(innerest_nodes_coordinate[i], innerest_univec_normal[i]);
    }
    if(inside_count == 0) break;
    DecisionOfInOut(number_of_innerest_nodes, innerest_nodes_coordinate);
  }

  for(i = 1; i < number_of_innerest_nodes-1; i++){
    AddNewNode(innerest_nodes_coordinate[i], innerest_univec_normal[i] ,CRACK_FACE, CRACK_SURFACE);
  }
}

/*
 * き裂面の点群を作る関数
 * 前のステップで一時的に保存した点群に点フラグを与える
 * まずき裂前縁の最も内側の層から一定距離以内にある点群の点フラグを削除
 * 次にき裂表面の点群から一定距離以内にある点群の点フラグを削除
 * それでも点フラグが残った点を順番に探索していき、
 * 点フラグが残った点を選択したらその点の近くの点フラグを削除、
 * を繰り返すことでき裂面に均一な密度の点群を生成する
 */
void LaminationPointsToNodes()
{
  int i, j;
  double dist;
  
  for(i = 0; i < number_of_inner_nodes; i++){
    for(j = 0; j < temp_count; j++){
      if(new_nodes_flag[j] == 1){
        dist = Distance(inner_nodes_coordinate[i], temp_crackface_new_nodes_coordinate[j]);
        if(dist < layer_size){
                    new_nodes_flag[j] = 0;
        }
      }
    }
  }
  for(i = 0; i < number_of_innerest_nodes; i++){
    for(j = 0; j < temp_count; j++){
      if(new_nodes_flag[j] == 1){
        dist = Distance(innerest_nodes_coordinate[i], temp_crackface_new_nodes_coordinate[j]);
        if(dist < layer_size*0.8){
          new_nodes_flag[j] = 0;
        }
      }
    }
  }
  for(i = 0; i < temp_count; i++){
    if(new_nodes_flag[i] == 1){
      AddNewNode(temp_crackface_new_nodes_coordinate[i], temp_crackface_new_normal_vector[i], CRACK_FACE, OTHER);
      for(j = 0; j < temp_count; j++){
        if(new_nodes_flag[j] == 1){
          dist = Distance(temp_crackface_new_nodes_coordinate[i], temp_crackface_new_nodes_coordinate[j]);
          if(dist < layer_size){
            new_nodes_flag[j] = 0;
          }
        }
      }
    }
  }
}

/*
 * き裂面の点群を生成する関数
 * 1. き裂前縁の.nodeへの登録
 * 2. 始点から終点へかけて隣り合う点でのベクトル生成
 * 3. 2.で生成したベクトルを利用して、各前縁点における接線方向ベクトル、法線方向ベクトル、進展方向ベクトルを作成
 * 4. ScaleFactor(積層距離を定めるパラメータ)を定義
 * 5. き裂前縁の始点と終点における進展方向ベクトルを表面パッチを利用して表面方向に補正
 * 6. き裂前縁から内側と外側に何層かずつ進展方向ベクトルを用いて積層（このプログラムでは内外それぞれ２層ずつ）
 * 7. 積層させたもののうち最も内側のものからさらに内側に向けて進展ベクトルを逐次計算し積層
 *    このとき、6.で用いた積層間距離の1/10とかなり細かく積層させ、それを保持
 *    表面パッチを利用して内外判定、内側へ積層させ続けて、全ての点が表面に達したら終了
 * 8. 7.で細かく保持させていた
 * のようなステップで構成される
 */
void PerformCommand()
{
  GetPointtoPointVector(nnodes, nodes_coordinate, p_to_p_vector);
  GetUnivectoratAllPoint(nnodes, p_to_p_vector, univec_normal, univec_propa, univec_tangent);
  SetScaleFactor();
  VectorCorrectiontoSurface();
  GetAverageVector(univec_propa[0], univec_propa[1], univec_propa[1]);
  GetAverageVector(univec_propa[nnodes-1], univec_propa[nnodes-2], univec_propa[nnodes-2]);
  ResisterCrackFrontNodes(nnodes, nodes_coordinate, univec_normal);
  MakeInOutLayer();

  printf("Laminate start\n");
  LaminationLayer();
  LaminationPointsToNodes();
}

int main(int argc, char *argv[]){
  ReadNodes(argv[1]);//き裂前縁点群の座標値を記した.nodeの読み込み
  ReadMapCrackNode(argv[2]);//き裂の一番端の点の法線ベクトルの情報
  ReadPatch(argv[3]);//き裂が入ってるメッシュファイルの表面パッチ.patch
  sprintf(patch_file_name, "%s", argv[3]);
  ReadCrackParam(argv[4]);//param.sc_ellipse_polygon3の読み込み

  PerformCommand();//き裂面点群の生成

  WriteCompleteNodes(argv[5]);//出来上がったき裂面の点群
  WriteCrackFlag(argv[6]);//き裂面、き裂前縁、それ以外に関するフラグ
  WriteSurfaceFlag(argv[7]);//き裂の表面とそれ以外に関するフラグ
  WriteCrackFrontFlag(argv[8]);//き裂前縁とそれ以外に関するフラグ
  WriteNormal(argv[9]);//き裂面に対して垂直な方向の各点におけるベクトル
  return 0;
}
