<?php
/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
include_once $GLOBALS['THRIFT_ROOT'].'/Thrift.php';

include_once $GLOBALS['THRIFT_ROOT'].'/packages/Client/Client_types.php';

class Hypertable_ThriftGen_HqlResult {
  static $_TSPEC;

  public $results = null;
  public $cells = null;
  public $scanner = null;
  public $mutator = null;

  public function __construct($vals=null) {
    if (!isset(self::$_TSPEC)) {
      self::$_TSPEC = array(
        1 => array(
          'var' => 'results',
          'type' => TType::LST,
          'etype' => TType::STRING,
          'elem' => array(
            'type' => TType::STRING,
            ),
          ),
        2 => array(
          'var' => 'cells',
          'type' => TType::LST,
          'etype' => TType::STRUCT,
          'elem' => array(
            'type' => TType::STRUCT,
            'class' => 'Hypertable_ThriftGen_Cell',
            ),
          ),
        3 => array(
          'var' => 'scanner',
          'type' => TType::I64,
          ),
        4 => array(
          'var' => 'mutator',
          'type' => TType::I64,
          ),
        );
    }
    if (is_array($vals)) {
      if (isset($vals['results'])) {
        $this->results = $vals['results'];
      }
      if (isset($vals['cells'])) {
        $this->cells = $vals['cells'];
      }
      if (isset($vals['scanner'])) {
        $this->scanner = $vals['scanner'];
      }
      if (isset($vals['mutator'])) {
        $this->mutator = $vals['mutator'];
      }
    }
  }

  public function getName() {
    return 'HqlResult';
  }

  public function read($input)
  {
    $xfer = 0;
    $fname = null;
    $ftype = 0;
    $fid = 0;
    $xfer += $input->readStructBegin($fname);
    while (true)
    {
      $xfer += $input->readFieldBegin($fname, $ftype, $fid);
      if ($ftype == TType::STOP) {
        break;
      }
      switch ($fid)
      {
        case 1:
          if ($ftype == TType::LST) {
            $this->results = array();
            $_size0 = 0;
            $_etype3 = 0;
            $xfer += $input->readListBegin($_etype3, $_size0);
            for ($_i4 = 0; $_i4 < $_size0; ++$_i4)
            {
              $elem5 = null;
              $xfer += $input->readString($elem5);
              $this->results []= $elem5;
            }
            $xfer += $input->readListEnd();
          } else {
            $xfer += $input->skip($ftype);
          }
          break;
        case 2:
          if ($ftype == TType::LST) {
            $this->cells = array();
            $_size6 = 0;
            $_etype9 = 0;
            $xfer += $input->readListBegin($_etype9, $_size6);
            for ($_i10 = 0; $_i10 < $_size6; ++$_i10)
            {
              $elem11 = null;
              $elem11 = new Hypertable_ThriftGen_Cell();
              $xfer += $elem11->read($input);
              $this->cells []= $elem11;
            }
            $xfer += $input->readListEnd();
          } else {
            $xfer += $input->skip($ftype);
          }
          break;
        case 3:
          if ($ftype == TType::I64) {
            $xfer += $input->readI64($this->scanner);
          } else {
            $xfer += $input->skip($ftype);
          }
          break;
        case 4:
          if ($ftype == TType::I64) {
            $xfer += $input->readI64($this->mutator);
          } else {
            $xfer += $input->skip($ftype);
          }
          break;
        default:
          $xfer += $input->skip($ftype);
          break;
      }
      $xfer += $input->readFieldEnd();
    }
    $xfer += $input->readStructEnd();
    return $xfer;
  }

  public function write($output) {
    $xfer = 0;
    $xfer += $output->writeStructBegin('HqlResult');
    if ($this->results !== null) {
      if (!is_array($this->results)) {
        throw new TProtocolException('Bad type in structure.', TProtocolException::INVALID_DATA);
      }
      $xfer += $output->writeFieldBegin('results', TType::LST, 1);
      {
        $output->writeListBegin(TType::STRING, count($this->results));
        {
          foreach ($this->results as $iter12)
          {
            $xfer += $output->writeString($iter12);
          }
        }
        $output->writeListEnd();
      }
      $xfer += $output->writeFieldEnd();
    }
    if ($this->cells !== null) {
      if (!is_array($this->cells)) {
        throw new TProtocolException('Bad type in structure.', TProtocolException::INVALID_DATA);
      }
      $xfer += $output->writeFieldBegin('cells', TType::LST, 2);
      {
        $output->writeListBegin(TType::STRUCT, count($this->cells));
        {
          foreach ($this->cells as $iter13)
          {
            $xfer += $iter13->write($output);
          }
        }
        $output->writeListEnd();
      }
      $xfer += $output->writeFieldEnd();
    }
    if ($this->scanner !== null) {
      $xfer += $output->writeFieldBegin('scanner', TType::I64, 3);
      $xfer += $output->writeI64($this->scanner);
      $xfer += $output->writeFieldEnd();
    }
    if ($this->mutator !== null) {
      $xfer += $output->writeFieldBegin('mutator', TType::I64, 4);
      $xfer += $output->writeI64($this->mutator);
      $xfer += $output->writeFieldEnd();
    }
    $xfer += $output->writeFieldStop();
    $xfer += $output->writeStructEnd();
    return $xfer;
  }

}

?>
