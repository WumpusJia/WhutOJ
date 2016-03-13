<?php
class submitController extends Controller 
{
    public function __construct() 
    {
        parent::__construct(__CLASS__);
    }

    public function index($arr)
    {
		if($arr) $problem_id = $arr['problem_id'];
		else $problem_id = 1000;
        $script = $this->convertScript(array('submit'));
        $data = array('page_title'=>'Submit','script'=>$script,'problem_id'=>$problem_id);
        $this->showTemplate('submit',$data);
    }
}

?>
